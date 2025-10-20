#include <windows.h>
#include <cassert>
#include "char_judge.h"

/**
 * @brief サロゲートペアを単一のUnicodeコードポイントにデコードします。
 * * @param highSurrogate 上位サロゲート (0xD800 - 0xDBFF)
 * @param lowSurrogate 下位サロゲート (0xDC00 - 0xDFFF)
 * @return UINT デコードされたUnicodeコードポイント (U+10000 - U+10FFFF)
 * @throws std::invalid_argument 有効なサロゲートペアでない場合
 */
UINT decode_surrogate_pair(wchar_t highSurrogate, wchar_t lowSurrogate)
{
    // Win32 APIで有効性をチェック
    assert(IS_SURROGATE_PAIR(highSurrogate, lowSurrogate));

    // デコードの公式: CodePoint = 0x10000 + (H - 0xD800) * 0x400 + (L - 0xDC00)
    // H: High Surrogate, L: Low Surrogate
    return 0x10000 + ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00);
}

bool is_surrogate_pair_kanji(wchar_t high, wchar_t low) {
    UINT code_point = decode_surrogate_pair(high, low);
    // U+20000 .. U+3FFFF
    return (0x20000 <= code_point && code_point <= 0x3FFFF);
}

bool is_surrogate_pair_kana(wchar_t high, wchar_t low) {
    UINT code_point = decode_surrogate_pair(high, low);
    // U+1B000 .. U+1B0FF
    UINT KANA_SUPPLEMENT_START = 0x1B000;
    UINT KANA_SUPPLEMENT_END   = 0x1B0FF;
    return (KANA_SUPPLEMENT_START <= code_point && code_point <= KANA_SUPPLEMENT_END);
}

size_t skip_one_real_char(const std::wstring& str, size_t& ich) {
    if (ich >= str.length())
        return 0;

    wchar_t ch = str[ich];
    if (IS_HIGH_SURROGATE(ch) && ich + 1 < str.length() && IS_LOW_SURROGATE(str[ich + 1])) {
        ich += 2;
        return 2;
    }

    ++ich;
    return 1;
}

size_t skip_kanji_chars(const std::wstring& str, size_t& ich) {
    if (ich >= str.length())
        return 0;

    size_t ret = 0;
    do {
        wchar_t ch = str[ich];
        if (IS_HIGH_SURROGATE(ch) && ich + 1 < str.length() && IS_LOW_SURROGATE(str[ich + 1])) {
            wchar_t high = ch;
            wchar_t low = str[ich + 1];
            if (!is_surrogate_pair_kanji(high, low))
                break;

            ++ich;
            ++ret;
        } else {
            if (!is_char_kanji(ch))
                break;
        }

        ++ich;
        ++ret;
    } while (ich < str.length());

    return ret;
}

size_t skip_kana_chars(const std::wstring& str, size_t& ich) {
    if (ich >= str.length())
        return 0;

    size_t ret = 0;
    do {
        wchar_t ch = str[ich];
        if (IS_HIGH_SURROGATE(ch) && ich + 1 < str.length() && IS_LOW_SURROGATE(str[ich + 1])) {
            wchar_t high = ch;
            wchar_t low = str[ich + 1];
            if (!is_surrogate_pair_kana(high, low))
                break;
            ++ich;
            ++ret;
        } else {
            if (!is_char_kana(ch))
                break;
        }

        ++ich;
        ++ret;
    } while (ich < str.length());

    return ret;
}

// 単語境界を検出する簡易関数
bool is_ascii_word_char(wchar_t ch) {
    // 英数字・アンダースコア・アポストロフィ・ハイフンなど
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9') ||
           ch == L'-' || ch == L'\'' || ch == L'_';
}

bool is_space_char(wchar_t ch) {
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

/**
 * find_word_boundary
 * 現在のインデックス index から単語の末尾（または先頭）を探す。
 *  action = +1 → 次の単語の末尾
 *  action = -1 → 前の単語の先頭
 *  count = 全体の文字数
 */
INT find_word_boundary(const std::wstring& text, INT index, INT count, INT action) {
    if (index < 0 || index >= count) return index;

    if (action > 0) {
        // 次の単語末尾を探す
        bool inWord = is_ascii_word_char(text[index]);
        while (index < count) {
            bool now = is_ascii_word_char(text[index]);
            if (inWord && !now) break;
            if (!inWord && now) break;
            index++;
        }
    } else {
        // 前の単語先頭を探す
        bool inWord = is_ascii_word_char(text[index]);
        while (index > 0) {
            bool now = is_ascii_word_char(text[index - 1]);
            if (inWord && !now) break;
            if (!inWord && now) break;
            index--;
        }
    }

    return index;
}
