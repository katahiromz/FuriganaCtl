#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <vector>

// 補助関数
constexpr inline bool is_char_hiragana(wchar_t ch) noexcept {
    return ((L'ぁ' <= ch && ch <= L'ん') || ch == L'ー' || ch == L'ゔ');
}
constexpr inline bool is_char_katakana(wchar_t ch) noexcept {
    return ((L'ァ' <= ch && ch <= L'ン') || ch == L'ー' || ch == L'ヴ');
}
constexpr inline bool is_char_kana(wchar_t ch) noexcept {
    return is_char_hiragana(ch) || is_char_katakana(ch);
}
constexpr inline bool is_char_kanji(wchar_t ch) noexcept {
    return ((0x3400 <= ch && ch <= 0x9FFF) || (0xF900 <= ch && ch <= 0xFAFF) || ch == 0x3007);
}

// 一行のフリガナ付きテキストを描画する。
INT APIENTRY DrawFuriganaTextLine(HDC dc, LPCWSTR compound_text, INT compound_text_len, UINT flags, LPRECT prc, HFONT hBaseFont, HFONT hRubyFont) noexcept;
