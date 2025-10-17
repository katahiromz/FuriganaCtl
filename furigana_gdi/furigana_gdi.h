#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include "pstdint.h"

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

    INT base_width;
    INT ruby_width;
    INT part_width;

    bool selected;

    TextPart() {
        part_width = -1;
        selected = false;
    }
};

void SetPartsSelection(std::vector<TextPart>& parts, INT iStart, INT iEnd);
INT HitTestTextPart(const std::vector<TextPart>& parts, INT x, INT y);

bool ParseRubyCompoundText(std::vector<TextPart>& parts, const std::wstring& text);

size_t DrawFuriganaOneLineText(
    HDC dc,
    const std::wstring& text,
    std::vector<TextPart>& parts,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    INT& delta_x,
    UINT flags);
