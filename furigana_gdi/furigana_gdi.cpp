#include "furigana_gdi.h"

#undef min
#undef max

// テキストの幅を計測する
INT get_text_width(HDC dc, LPCWSTR text, INT len) noexcept {
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
    INT start_index;

    // ベーステキスト（通常のテキスト、またはルビブロックの漢字）
    LPCWSTR base_text;
    INT base_len;

    // ルビテキスト（ルビブロックのフリガナ）
    LPCWSTR ruby_text;
    INT ruby_len;
};

// ルビコンパウンドテキストをパーツに分解する
bool ParseRubyCompoundText(std::vector<TextPart>& parts, LPCWSTR text, INT len) noexcept {
    INT i = 0;
    bool has_ruby = false;
    while (i < len) {
        // "{漢字(かんじ)}"
        if (text[i] == '{') {
            const wchar_t *paren_start = wcschr(&text[i], '(');
            if (paren_start) {
                const wchar_t* furigana_end = wcsstr(paren_start, L")}");
                if (furigana_end) {
                    parts.push_back({ TextPart::RUBY, i + 1, &text[i + 1], (paren_start - &text[i + 1]), paren_start + 1, (furigana_end - (paren_start + 1)) });
                    i = (furigana_end - text) + 2;
                    has_ruby = true;
                    continue;
                }
            }
        }
        // "漢字(かんじ)"
        INT kanji_start = i;
        INT kanji_end = i;
        while (kanji_end < len && is_char_kanji(text[kanji_end])) {
            kanji_end++;
        }
        if (kanji_end > kanji_start && kanji_end < len && text[kanji_end] == L'(') {
            INT ruby_start = kanji_end + 1;
            INT ruby_end = ruby_start;
            while (ruby_end < len && text[ruby_end] != L')') {
                if (!is_char_kana(text[ruby_end])) break;
                ruby_end++;
            }
            if (ruby_end > ruby_start && ruby_end < len && text[ruby_end] == L')') {
                parts.push_back({TextPart::RUBY, kanji_start, text + kanji_start, kanji_end - kanji_start, text + ruby_start, ruby_end - ruby_start});
                i = ruby_end + 1;
                has_ruby = true;
                continue;
            }
        }
        INT normal_start = i;
        INT normal_end = i;
        while (normal_end < len) {
            if (is_char_kanji(text[normal_end]) || text[normal_end] == L'(' || text[normal_end] == L'{') {
                if (normal_end > normal_start) break;
            }
            normal_end++;
        }
        parts.push_back({TextPart::NORMAL, normal_start, text + normal_start, normal_end - normal_start, nullptr, 0});
        i = normal_end;
    }
    return has_ruby;
}

/**
 * 一行のフリガナ付きテキストを描画する。
 *
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは nullptr。
 * @param compound_text 描画したい、一行のルビ コンパウンド テキスト。
 * @param compound_text_len 描画する一行のルビ コンパウンド テキストの長さ（文字単位）。
 * @param flags 未使用。
 * @param prc 描画する位置とサイズ。描画または計測後はサイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @return 折り返しがない場合は -1。折り返しがある場合は、折り返し場所のルビ コンパウンド テキストの文字インデックス。
 */
INT APIENTRY DrawFuriganaTextLine(HDC dc, LPCWSTR compound_text, INT compound_text_len, UINT flags, LPRECT prc, HFONT hBaseFont, HFONT hRubyFont) noexcept
{
    if (compound_text_len <= 0) {
        prc->right = prc->left;
        prc->bottom = prc->top;
        return -1;
    }

    std::vector<TextPart> parts;
    bool has_ruby = ParseRubyCompoundText(parts, compound_text, compound_text_len);
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
    INT two_char_width = get_text_width(hdc, L"漢字", 2);
    SelectObject(hdc, hFontOld);

    // 3. パーツを順に処理し、幅を計測/描画する
    for (const auto& part : parts) {
        INT base_width, ruby_width = 0, part_width;

        // 幅の計測
        if (part.type == TextPart::NORMAL) {
            hFontOld = SelectObject(hdc, hBaseFont);
            base_width = part_width = get_text_width(hdc, part.base_text, part.base_len);
            SelectObject(hdc, hFontOld);
        } else { // TextPart::RUBY
            hFontOld = SelectObject(hdc, hBaseFont);
            base_width = get_text_width(hdc, part.base_text, part.base_len);
            SelectObject(hdc, hRubyFont);
            ruby_width = get_text_width(hdc, part.ruby_text, part.ruby_len);
            SelectObject(hdc, hFontOld);

            // ルビブロックの幅は、ベースとルビの幅の大きい方
            part_width = std::max(base_width, ruby_width);
        }

        // 4. 折り返し判定
        if (current_x + part_width > prc->right) {
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
                ExtTextOutW(dc, current_x, base_y, 0, nullptr, part.base_text, part.base_len, nullptr);
                SelectObject(dc, hFontOld);
            } else { // TextPart::RUBY
                // A. ベーステキストの描画
                hFontOld = SelectObject(dc, hBaseFont);

                INT base_start_x = current_x;
                if (part_width > base_width) {
                    base_start_x += (part_width - base_width) / 2;
                }

                ExtTextOutW(dc, base_start_x, base_y, 0, nullptr, part.base_text, part.base_len, nullptr);

                SelectObject(dc, hFontOld);

                // B. ルビテキストの描画 (SetTextCharacterExtraを使用)
                hFontOld = SelectObject(dc, hRubyFont);

                INT ruby_extra = 0;
                INT ruby_start_x = current_x;

                if (part_width - two_char_width > ruby_width) {
                    if (part.ruby_len > 1) {
                        ruby_extra = (part_width - ruby_width) / (part.ruby_len - 1);
                    }
                } else {
                    ruby_start_x += (part_width - ruby_width) / 2;
                }

                INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                ExtTextOutW(dc, ruby_start_x, prc->top, 0, nullptr, part.ruby_text, part.ruby_len, nullptr);
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
