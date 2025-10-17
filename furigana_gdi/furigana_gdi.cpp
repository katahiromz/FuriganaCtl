#include "furigana_gdi.h"
#include "char_judge.h"
#include <assert.h>

#undef min
#undef max

// テキストの幅を計測する
INT get_text_width(HDC dc, LPCWSTR text, INT len) {
    if (len <= 0) return 0;
    SIZE size = {0};
    GetTextExtentPoint32W(dc, text, len, &size);
    return size.cx;
}

// ルビコンパウンドテキストをパーツに分解する
bool ParseRubyCompoundText(std::vector<TextPart>& parts, const std::wstring& text) {
    parts.clear();

    size_t ich = 0;
    bool has_ruby = false;
    while (ich < text.length()) {
        // "{ベーステキスト(ルビテキスト)}"
        if (text[ich] == L'{') {
            intptr_t paren_start = text.find(L'(', ich);
            if (paren_start != text.npos) {
                intptr_t furigana_end = text.find(L")}", paren_start);
                if (furigana_end != text.npos) {
                    TextPart part;
                    part.type = TextPart::RUBY;
                    part.start_index = ich;
                    part.end_index = furigana_end + 2;
                    part.base_index = ich + 1;
                    part.base_len = paren_start - (ich + 1);
                    part.ruby_index = paren_start + 1;
                    part.ruby_len = furigana_end - (paren_start + 1);
                    parts.push_back(part);
                    ich = furigana_end + 2;
                    has_ruby = true;
                    continue;
                }
            }
        }

        // "漢字(ふりがな)"
        size_t ich0 = ich; // 漢字の始まり？
        size_t kanji_len = skip_kanji_chars(text, ich);
        if (kanji_len > 0) {
            if (ich < text.length() && text[ich] == L'(') { // 漢字の次に半角の丸カッコがある？
                ++ich;
                size_t ich1 = ich; // フリガナの始まり？
                size_t kana_len = skip_kana_chars(text, ich);
                if (kana_len > 0) { // 丸カッコの後にカナがある？
                    size_t ich2 = ich; // フリガナの終わり？
                    if (ich2 < text.length() && text[ich2] == L')') { // フリガナの次に「丸カッコ閉じる」がある？
                        TextPart part;
                        part.type = TextPart::RUBY;
                        part.start_index = ich0;
                        part.end_index = ich2 + 1;
                        part.base_index = ich0;
                        part.base_len = (ich1 - 1) - ich0;
                        part.ruby_index = ich1;
                        part.ruby_len = ich2 - ich1;
                        parts.push_back(part);
                        ich = ich2 + 1;
                        has_ruby = true;
                        continue;
                    }
                }
            }
            ich = ich0;
        }

        size_t char_index = ich;
        size_t char_len = skip_one_real_char(text, ich);
        TextPart part;
        part.type = TextPart::NORMAL;
        part.start_index = char_index;
        part.end_index = ich;
        part.base_index = char_index;
        part.base_len = char_len;
        part.ruby_index = 0;
        part.ruby_len = 0;
        parts.push_back(part);
    }
    return has_ruby;
}

struct MetricInfo {
    INT x_extent; // [out]
    INT y_extent; // [out]
    size_t iPart; // [out]
    INT ruby_height; // [out]
    INT max_width; // [in]
    bool has_ruby; // [out]
};

void
CalcTextPart(
    const std::wstring& text,
    TextPart& part,
    HDC dc,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    if (part.part_width != -1)
        return; // 計算済み

    // 幅の計測
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);
    if (part.type == TextPart::NORMAL) {
        part.base_width = get_text_width(dc, &text[part.base_index], part.base_len);
        part.part_width = part.base_width;
        part.ruby_width = 0;
    } else { // TextPart::RUBY
        part.base_width = get_text_width(dc, &text[part.base_index], part.base_len);
        SelectObject(dc, hRubyFont);
        part.ruby_width = get_text_width(dc, &text[part.ruby_index], part.ruby_len);

        // ルビブロックの幅は、ベースとルビの幅の大きい方
        part.part_width = std::max(part.base_width, part.ruby_width);
    }
    SelectObject(dc, hFontOld);
}

INT
CalcTextHeight(
    HDC dc,
    INT& base_height,
    INT& ruby_height,
    bool has_ruby,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);
    TEXTMETRICW tm_base;
    GetTextMetricsW(dc, &tm_base);
    base_height = tm_base.tmHeight;

    INT y_extent;
    if (has_ruby) {
        SelectObject(dc, hRubyFont);
        TEXTMETRICW tm_ruby;
        GetTextMetricsW(dc, &tm_ruby);
        ruby_height = tm_ruby.tmHeight;
        // ルビ付き行の高さ: ルビの高さ + ベースの高さ
        y_extent = ruby_height + base_height;
    } else {
        ruby_height = 0;
        y_extent = base_height;
    }
    SelectObject(dc, hFontOld);

    return y_extent;
}

size_t
GetRubyCompoundMetric(
    MetricInfo& metric,
    const std::wstring& text,
    std::vector<TextPart>& parts,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    // metricのメンバーを参照
    INT& x_extent = metric.x_extent;
    INT& y_extent = metric.y_extent;
    size_t& iPart = metric.iPart;
    INT& ruby_height = metric.ruby_height;
    INT& max_width = metric.max_width;
    bool& has_ruby = metric.has_ruby;

    HDC dc = CreateCompatibleDC(NULL);

    // ルビがあるか？
    for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = parts[iPart];
        if (part.type == TextPart::RUBY && part.ruby_len > 0) {
            has_ruby = true;
        }
    }

    // フォントメトリクスと行の高さを決定
    INT base_height;
    y_extent = CalcTextHeight(dc, base_height, ruby_height, has_ruby, hBaseFont, hRubyFont);

    x_extent = 0;
    for (iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = parts[iPart];

        // パートを計算
        CalcTextPart(text, part, dc, hBaseFont, hRubyFont);

        // 最大幅を超えたら、折り返し。
        // TODO: 禁則処理
        if (max_width >= 0 && x_extent + part.part_width > max_width && x_extent == 0) {
            break;
        }

        x_extent += part.part_width;
    }

    DeleteDC(dc);
    return iPart;
}

/**
 * 一行のフリガナ付きテキストを描画する。
 *
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param text 描画したい、一行のルビ コンパウンド テキスト。
 * @param prc 描画する位置とサイズ。計測後はサイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @param flags 次のフラグを使用可能: DT_NOCLIP
 * @return 折り返しがない場合は std::wstring::npos。折り返しがある場合は、折り返し場所のパートのインデックス。
 */
size_t DrawFuriganaOneLineText(
    HDC dc,
    const std::wstring& text,
    std::vector<TextPart>& parts,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags)
{
    if (text.length() <= 0 || parts.empty()) {
        prc->right = prc->left;
        prc->bottom = prc->top;
        return std::wstring::npos;
    }

    // dcがNULLの場合、計測用に画面のHDCを使用する
    HDC hdc = dc ? dc : CreateCompatibleDC(NULL);
    if (!hdc) {
        assert(0);
        prc->right = prc->left;
        prc->bottom = prc->top;
        return std::wstring::npos;
    }

    // 描画/計測の準備
    INT saved_dc = 0;
    if (dc && !(flags & DT_NOCLIP)) {
        saved_dc = SaveDC(hdc);
        IntersectClipRect(hdc, prc->left, prc->top, prc->right, prc->bottom);
    }
    if (dc) SetBkMode(dc, TRANSPARENT); // 背景モード設定

    // メトリック情報を取得
    MetricInfo metric;
    metric.max_width = (flags & DT_SINGLELINE) ? -1 : (prc->right - prc->left);
    size_t break_iPart = GetRubyCompoundMetric(metric, text, parts, hBaseFont, hRubyFont);

    INT ruby_height = metric.ruby_height;
    INT line_width = metric.x_extent, block_height = metric.y_extent;
    INT max_width = metric.max_width;

    INT current_x = prc->left;
    if (flags & DT_CENTER)
        current_x += (max_width - line_width) / 2;
    else if (flags & DT_RIGHT)
        current_x += (max_width - line_width);

    const INT base_y = prc->top + ruby_height; // ベーステキストのY座標

    // しきい値を取得する
    HGDIOBJ hFontOld = SelectObject(hdc, hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢i", 2);
    SelectObject(hdc, hFontOld);

    // パーツを順に処理し、計測または描画する
    for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = parts[iPart];

        if (break_iPart == iPart) // 折り返し判定
            break;

        // 必要なら再計算
        CalcTextPart(text, part, dc, hBaseFont, hRubyFont);

        // 描画対象の長方形
        RECT rc = { current_x, prc->top, current_x + part.part_width, prc->top + block_height };

        if (part.selected) { // 選択状態か？
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }

        if (dc && RectVisible(dc, &rc)) { // 描画すべきか？
            if (part.type == TextPart::NORMAL) {
                // 通常テキストの描画 (ベースラインに描画)
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, current_x, base_y, 0, NULL, &text[part.base_index], part.base_len, NULL);
                SelectObject(dc, hFontOld);
            } else { // TextPart::RUBY
                // ベーステキストの配置を決める
                INT base_start_x = current_x;
                if (part.part_width > part.base_width) {
                    base_start_x += (part.part_width - part.base_width) / 2;
                }

                // ルビの配置を決める
                INT ruby_extra, ruby_start_x = current_x;
                if (part.part_width - part.ruby_width > gap_threshold) {
                    // 両端ぞろえ
                    // SetTextCharacterExtraで使用する文字間スペースを設定
                    if (part.ruby_len > 1) {
                        ruby_extra = (part.part_width - part.ruby_width) / (part.ruby_len - 1);
                    }
                } else {
                    // パート内で中央ぞろえ
                    ruby_start_x += (part.part_width - part.ruby_width) / 2;
                    ruby_extra = 0;
                }

                // ベーステキストの描画
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &text[part.base_index], part.base_len, NULL);
                SelectObject(dc, hFontOld);

                // ルビテキストの描画
                hFontOld = SelectObject(dc, hRubyFont);
                INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &text[part.ruby_index], part.ruby_len, NULL);
                SetTextCharacterExtra(dc, old_extra_ruby);
                SelectObject(dc, hFontOld);
            }
        }

        // 次の描画位置に更新
        current_x += part.part_width;
    }

    // prc のサイズを更新
    prc->right = current_x;
    prc->bottom = prc->top + block_height;

    // クリーンアップ
    if (dc && !(flags & DT_NOCLIP)) RestoreDC(hdc, saved_dc);
    if (!dc) DeleteDC(hdc);

    return break_iPart;
}
