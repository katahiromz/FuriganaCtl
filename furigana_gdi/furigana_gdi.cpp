#include "furigana_gdi.h"
#include "char_judge.h"

#undef min
#undef max

// テキストの幅を計測する
INT get_text_width(HDC dc, LPCWSTR text, INT len) {
    if (len <= 0) return 0;
    SIZE size = {0};
    GetTextExtentPoint32W(dc, text, len, &size);
    return size.cx;
}

// テキストの要素を表す構造体
struct TextPart {
    enum Type {
        NORMAL, // 通常テキスト
        RUBY    // ルビブロック
    } type;

    // 元の compound_text 内での開始インデックス
    size_t start_index;
    size_t end_index;

    size_t base_index;
    size_t base_len;

    size_t ruby_index;
    size_t ruby_len;
};

// ルビコンパウンドテキストをパーツに分解する
bool ParseRubyCompoundText(std::vector<TextPart>& parts, const std::wstring& text) {
    size_t ich = 0;
    bool has_ruby = false;
    while (ich < text.length()) {
        // "{ベーステキスト(ルビテキスト)}"
        if (text[ich] == L'{') {
            intptr_t paren_start = text.find(L'(', ich);
            if (paren_start != text.npos) {
                intptr_t furigana_end = text.find(L")}", paren_start);
                if (furigana_end != text.npos) {
                    TextPart part = { TextPart::RUBY };
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
                        TextPart part = { TextPart::RUBY };
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
        TextPart part = { TextPart::NORMAL };
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

/**
 * 一行のフリガナ付きテキストを描画する。
 *
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは nullptr。
 * @param compound_text 描画したい、一行のルビ コンパウンド テキスト。
 * @param flags 未使用。
 * @param prc 描画する位置とサイズ。描画または計測後はサイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @return 折り返しがない場合は -1。折り返しがある場合は、折り返し場所のルビ コンパウンド テキストの文字インデックス。
 */
INT DrawFuriganaTextLine(
    HDC dc,
    const std::wstring& compound_text,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags)
{
    const std::wstring& text = compound_text;

    if (text.length() <= 0) {
        prc->right = prc->left;
        prc->bottom = prc->top;
        return -1;
    }

    std::vector<TextPart> parts;
    bool has_ruby = ParseRubyCompoundText(parts, text);
    if (parts.empty()) return -1;

    // dcがnullptrの場合、計測用に画面のHDCを使用する
    HDC hdc = dc ? dc : GetDC(nullptr);
    if (!hdc) return -1;

    // 1. フォントメトリクスと行の高さを決定
    INT base_height, ruby_height, block_height;
    HGDIOBJ hFontOld = SelectObject(hdc, hBaseFont);
    TEXTMETRICW tm_base;
    GetTextMetricsW(hdc, &tm_base);
    base_height = tm_base.tmHeight;

    if (has_ruby) {
        SelectObject(hdc, hRubyFont);
        TEXTMETRICW tm_ruby;
        GetTextMetricsW(hdc, &tm_ruby);
        ruby_height = tm_ruby.tmHeight;
        // ルビ付き行の高さ: ルビの高さ + ベースの高さ
        block_height = ruby_height + base_height;
    } else {
        ruby_height = 0;
        block_height = base_height;
    }
    SelectObject(hdc, hFontOld); // フォントを元に戻す

    // 2. 描画/計測の準備
    INT saved_dc = 0;
    if (dc && !(flags & DT_NOCLIP)) {
        saved_dc = SaveDC(hdc);
        IntersectClipRect(hdc, prc->left, prc->top, prc->right, prc->bottom);
    }
    if (dc) SetBkMode(dc, TRANSPARENT); // 背景モード設定

    const INT max_width = prc->right - prc->left;
    INT current_x = prc->left;
    INT wrap_index = -1;
    const INT base_y = prc->top + ruby_height; // ベーステキストのY座標

    hFontOld = SelectObject(hdc, hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢字i", 2);
    SelectObject(hdc, hFontOld);

    // 3. パーツを順に処理し、幅を計測/描画する
    for (const auto& part : parts) {
        INT base_width, ruby_width = 0, part_width;

        // 幅の計測
        if (part.type == TextPart::NORMAL) {
            hFontOld = SelectObject(hdc, hBaseFont);
            base_width = part_width = get_text_width(hdc, &text[part.base_index], part.base_len);
            SelectObject(hdc, hFontOld);
        } else { // TextPart::RUBY
            hFontOld = SelectObject(hdc, hBaseFont);
            base_width = get_text_width(hdc, &text[part.base_index], part.base_len);
            SelectObject(hdc, hRubyFont);
            ruby_width = get_text_width(hdc, &text[part.ruby_index], part.ruby_len);
            SelectObject(hdc, hFontOld);

            // ルビブロックの幅は、ベースとルビの幅の大きい方
            part_width = std::max(base_width, ruby_width);
        }

        // 4. 折り返し判定
        if (!(flags & DT_SINGLELINE) && current_x + part_width > prc->right) {
            // 最初のパーツ全体が収まらない場合を除き、折り返し位置を設定
            if (current_x > prc->left) {
                wrap_index = part.start_index;
                break;
            }
        }

        // 5. 描画 (dc != nullptr の場合)
        if (dc) {
            if (part.type == TextPart::NORMAL) {
                // 通常テキストの描画 (ベースラインに描画)
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, current_x, base_y, 0, nullptr, &text[part.base_index], part.base_len, nullptr);
                SelectObject(dc, hFontOld);
            } else { // TextPart::RUBY
                // A. ベーステキストの描画
                hFontOld = SelectObject(dc, hBaseFont);

                INT base_start_x = current_x;
                if (part_width > base_width) {
                    base_start_x += (part_width - base_width) / 2;
                }

                ExtTextOutW(dc, base_start_x, base_y, 0, nullptr, &text[part.base_index], part.base_len, nullptr);

                SelectObject(dc, hFontOld);

                // B. ルビテキストの描画 (SetTextCharacterExtraを使用)
                hFontOld = SelectObject(dc, hRubyFont);

                INT ruby_extra = 0;
                INT ruby_start_x = current_x;

                if (part_width - ruby_width > gap_threshold) {
                    if (part.ruby_len > 1) {
                        ruby_extra = (part_width - ruby_width) / (part.ruby_len - 1);
                    }
                } else {
                    ruby_start_x += (part_width - ruby_width) / 2;
                }

                INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                ExtTextOutW(dc, ruby_start_x, prc->top, 0, nullptr, &text[part.ruby_index], part.ruby_len, nullptr);
                SetTextCharacterExtra(dc, old_extra_ruby);

                SelectObject(dc, hFontOld);
            }
        }

        // 6. 次の描画位置に更新
        current_x += part_width;
    }

    // 7. prc のサイズを更新
    prc->right = current_x;
    prc->bottom = prc->top + block_height;

    // 8. クリーンアップ
    if (dc && !(flags & DT_NOCLIP)) {
        RestoreDC(hdc, saved_dc);
    }
    if (!dc) ReleaseDC(nullptr, hdc);

    // 9. 戻り値
    return wrap_index;
}
