#pragma once

#include <string>

// 補助関数
constexpr inline bool is_char_hiragana(wchar_t ch) {
    return ((L'ぁ' <= ch && ch <= L'ん') || ch == L'ー' || ch == L'ゔ');
}
constexpr inline bool is_char_katakana(wchar_t ch) {
    return ((L'ァ' <= ch && ch <= L'ン') || ch == L'ー' || ch == L'ヴ');
}
constexpr inline bool is_char_kana(wchar_t ch) {
    return is_char_hiragana(ch) || is_char_katakana(ch);
}
constexpr inline bool is_char_digit(wchar_t ch) {
    return ((L'0' <= ch && ch <= L'9') || (L'０' <= ch && ch <= L'９'));
}
constexpr inline bool is_char_lower(wchar_t ch) {
    return ((L'a' <= ch && ch <= L'z') || (L'ａ' <= ch && ch <= L'ｚ'));
}
constexpr inline bool is_char_upper(wchar_t ch) {
    return ((L'A' <= ch && ch <= L'Z') || (L'Ａ' <= ch && ch <= L'Ｚ'));
}
constexpr inline bool is_char_alpha(wchar_t ch) {
    return is_char_lower(ch) || is_char_upper(ch);
}
constexpr inline bool is_char_alpha_numeric(wchar_t ch) {
    return is_char_alpha(ch) || is_char_digit(ch);
}
constexpr inline bool is_char_kanji(wchar_t ch) {
    return ((0x3400 <= ch && ch <= 0x9FFF) || (0xF900 <= ch && ch <= 0xFAFF) || ch == 0x3007);
}

#define is_surrogate_pair(high, low) IS_SURROGATE_PAIR((high), (low))
bool is_surrogate_pair_kana(wchar_t high, wchar_t low);
bool is_surrogate_pair_kanji(wchar_t high, wchar_t low);
size_t skip_one_real_char(const std::wstring& str, size_t& ich);
size_t skip_kanji_chars(const std::wstring& str, size_t& ich);
size_t skip_kana_chars(const std::wstring& str, size_t& ich);
UINT decode_surrogate_pair(wchar_t highSurrogate, wchar_t lowSurrogate);
