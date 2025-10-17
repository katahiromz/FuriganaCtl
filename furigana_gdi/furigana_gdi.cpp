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

// テキストランの選択を更新する
void UpdateTextRunSelection(TextRun& run) {
    std::vector<TextPart>& parts = run.m_parts;
    INT iStart = run.m_selection_start;
    INT iEnd = run.m_selection_end;
    if (iStart == -1) {
        for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
            parts[iPart].m_selected = false;
        }
    } else {
        if (iEnd == -1)
            iEnd = (INT)parts.size();

        if (iStart > iEnd)
            std::swap(iStart, iEnd);

        for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
            parts[iPart].m_selected = (iStart <= (INT)iPart && (INT)iPart < iEnd);
        }
    }
}

// テキストランをパースする
bool ParseTextRun(TextRun& run, const std::wstring& text) {
    run.m_text = text;
    std::vector<TextPart>& parts = run.m_parts;
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
                    part.m_type = TextPart::RUBY;
                    part.m_start_index = ich;
                    part.m_end_index = furigana_end + 2;
#ifndef NDEBUG // デバッグ時のみ
                    part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
                    part.m_base_index = ich + 1;
                    part.m_base_len = paren_start - (ich + 1);
                    part.m_ruby_index = paren_start + 1;
                    part.m_ruby_len = furigana_end - (paren_start + 1);
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
                        part.m_type = TextPart::RUBY;
                        part.m_start_index = ich0;
                        part.m_end_index = ich2 + 1;
#ifndef NDEBUG // デバッグ時のみ
                        part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
                        part.m_base_index = ich0;
                        part.m_base_len = (ich1 - 1) - ich0;
                        part.m_ruby_index = ich1;
                        part.m_ruby_len = ich2 - ich1;
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
        part.m_type = TextPart::NORMAL;
        part.m_start_index = char_index;
        part.m_end_index = ich;
#ifndef NDEBUG // デバッグ時のみ
        part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
        part.m_base_index = char_index;
        part.m_base_len = char_len;
        part.m_ruby_index = 0;
        part.m_ruby_len = 0;
        parts.push_back(part);
    }
    return has_ruby;
}

void
UpdateTextRunHeight(
    HDC dc,
    TextRun& run,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);

    TEXTMETRICW tm_base;
    GetTextMetricsW(dc, &tm_base);
    run.m_base_height = tm_base.tmHeight;

    if (run.m_has_ruby) {
        SelectObject(dc, hRubyFont);
        TEXTMETRICW tm_ruby;
        GetTextMetricsW(dc, &tm_ruby);
        run.m_ruby_height = tm_ruby.tmHeight;

        // ルビ付き行の高さ: ルビの高さ + ベースの高さ
        run.m_run_height = run.m_ruby_height + run.m_base_height;
    } else {
        run.m_ruby_height = 0;
        run.m_run_height = run.m_base_height;
    }

    SelectObject(dc, hFontOld);
}

void
UpdateTextPartWidth(
    const std::wstring& text,
    TextPart& part,
    HDC dc,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    // 幅の計測
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);
    if (part.m_type == TextPart::NORMAL) {
        part.m_base_width = get_text_width(dc, &text[part.m_base_index], part.m_base_len);
        part.m_part_width = part.m_base_width;
        part.m_ruby_width = 0;
    } else { // TextPart::RUBY
        part.m_base_width = get_text_width(dc, &text[part.m_base_index], part.m_base_len);
        SelectObject(dc, hRubyFont);
        part.m_ruby_width = get_text_width(dc, &text[part.m_ruby_index], part.m_ruby_len);
        // ルビブロックの幅は、ベースとルビの幅の大きい方
        part.m_part_width = std::max(part.m_base_width, part.m_ruby_width);
    }
    SelectObject(dc, hFontOld);
}

size_t
UpdateTextRunWidth(HDC dc, TextRun& run, HFONT hBaseFont, HFONT hRubyFont) {
    INT x_extent = 0;
    INT max_width = run.m_max_width;

    std::vector<TextPart>& parts = run.m_parts;
    size_t iPart;
    for (iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = run.m_parts[iPart];

        // パートを計算
        UpdateTextPartWidth(run.m_text, part, dc, hBaseFont, hRubyFont);

        // 最大幅を超えたら、折り返し。
        // TODO: 禁則処理
        if (max_width > 0 && x_extent + part.m_part_width > max_width && x_extent != 0) {
            break;
        }

        x_extent += part.m_part_width;
    }

    run.m_run_width = x_extent;
    return iPart;
}

// テキストランの当たり判定
INT HitTestTextRun(const TextRun& run, INT x, INT y) {
    const std::vector<TextPart>& parts = run.m_parts;

    x -= run.m_delta_x; // 右そろえ、中央そろえの修正分

    INT current_x = 0;
    for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
        const TextPart& part = parts[iPart];
        current_x += part.m_part_width;
        if (x < current_x - part.m_part_width / 2)
            return (INT)iPart;
    }
    return (INT)parts.size();
}

size_t
GetTextRunMetric(
    TextRun& run,
    HFONT hBaseFont,
    HFONT hRubyFont)
{
    // ルビがあるか？
    std::vector<TextPart>& parts = run.m_parts;
    for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = run.m_parts[iPart];
        if (part.m_type == TextPart::RUBY && part.m_ruby_len > 0) {
            run.m_has_ruby = true;
        }
    }

    // 寸法を計算
    HDC dc = CreateCompatibleDC(NULL);
    UpdateTextRunHeight(dc, run, hBaseFont, hRubyFont);
    size_t iPart_break = UpdateTextRunWidth(dc, run, hBaseFont, hRubyFont);
    DeleteDC(dc);

    return iPart_break;
}

/**
 * 一行のフリガナ付きテキストを描画する。
 *
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param text 描画したい、一行のルビ コンパウンド テキスト。
 * @param prc 描画する位置とサイズ。計測後はサイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT, DT_NOCLIP。
 * @return 折り返しがない場合は std::wstring::npos。折り返しがある場合は、折り返し場所のパートのインデックス。
 */
size_t DrawTextRun(
    HDC dc,
    TextRun& run,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags)
{
    std::wstring& text = run.m_text;
    std::vector<TextPart>& parts = run.m_parts;

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
    run.m_max_width = (flags & DT_SINGLELINE) ? -1 : (prc->right - prc->left);
    size_t iPart_break = GetTextRunMetric(run, hBaseFont, hRubyFont);

    if (flags & DT_CENTER)
        run.m_delta_x = (run.m_max_width - run.m_run_width) / 2;
    else if (flags & DT_RIGHT)
        run.m_delta_x = run.m_max_width - run.m_run_width;
    else
        run.m_delta_x = 0;

    INT current_x = prc->left + run.m_delta_x;
    const INT base_y = prc->top + run.m_ruby_height; // ベーステキストのY座標

    // しきい値を取得する
    HGDIOBJ hFontOld = SelectObject(hdc, hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢i", 2);
    SelectObject(hdc, hFontOld);

    // パーツを順に処理し、計測または描画する
    for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
        TextPart& part = parts[iPart];

        if (iPart_break == iPart) // 折り返し判定
            break;

        // 必要なら再計算
        UpdateTextPartWidth(text, part, dc, hBaseFont, hRubyFont);

        // 描画対象の長方形
        RECT rc = { current_x, prc->top, current_x + part.m_part_width, prc->top + run.m_run_height };

        if (part.m_selected) { // 選択状態か？
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }

        if (dc && RectVisible(dc, &rc)) { // 描画すべきか？
            if (part.m_type == TextPart::NORMAL) {
                // 通常テキストの描画 (ベースラインに描画)
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, current_x, base_y, 0, NULL, &text[part.m_base_index], part.m_base_len, NULL);
                SelectObject(dc, hFontOld);
            } else { // TextPart::RUBY
                // ベーステキストの配置を決める
                INT base_start_x = current_x;
                if (part.m_part_width > part.m_base_width) {
                    base_start_x += (part.m_part_width - part.m_base_width) / 2;
                }

                // ルビの配置を決める
                INT ruby_extra, ruby_start_x = current_x;
                if (part.m_part_width - part.m_ruby_width > gap_threshold) {
                    // 両端ぞろえ
                    // SetTextCharacterExtraで使用する文字間スペースを設定
                    if (part.m_ruby_len > 1) {
                        ruby_extra = (part.m_part_width - part.m_ruby_width) / (part.m_ruby_len - 1);
                    }
                } else {
                    // パート内で中央ぞろえ
                    ruby_start_x += (part.m_part_width - part.m_ruby_width) / 2;
                    ruby_extra = 0;
                }

                // ベーステキストの描画
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &text[part.m_base_index], part.m_base_len, NULL);
                SelectObject(dc, hFontOld);

                // ルビテキストの描画
                hFontOld = SelectObject(dc, hRubyFont);
                INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &text[part.m_ruby_index], part.m_ruby_len, NULL);
                SetTextCharacterExtra(dc, old_extra_ruby);
                SelectObject(dc, hFontOld);
            }
        }

        // 次の描画位置に更新
        current_x += part.m_part_width;
    }

    if (!dc) { // 計測なら prc のサイズを更新
        prc->right = current_x;
        prc->bottom = prc->top + run.m_run_height;
    }

    // クリーンアップ
    if (dc && !(flags & DT_NOCLIP)) RestoreDC(hdc, saved_dc);
    if (!dc) DeleteDC(hdc);

    return iPart_break;
}
