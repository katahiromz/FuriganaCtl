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
    } m_type;

#ifndef NDEBUG // デバッグ時のみ
    std::wstring m_text;
#endif

    // 元の compound_text 内での開始インデックス
    size_t m_start_index;
    size_t m_end_index;

    size_t m_base_index;
    size_t m_base_len;

    size_t m_ruby_index;
    size_t m_ruby_len;

    INT m_base_width;
    INT m_ruby_width;
    INT m_part_width;

    bool m_selected;

    TextPart() {
        m_part_width = -1;
        m_selected = false;
    }
};

// テキストの連続
struct TextRun {
    std::wstring m_text;
    std::vector<TextPart> m_parts;
    INT m_selection_start;
    INT m_selection_end;
    INT m_base_height;
    INT m_ruby_height;
    INT m_run_width;
    INT m_run_height;
    INT m_max_width;
    INT m_delta_x;
    bool m_has_ruby;

    TextRun() {
        m_delta_x = 0;
        m_selection_start = -1;
        m_selection_end = -1;
        m_base_height = 0;
        m_ruby_height = 0;
        m_has_ruby = false;
    }
};

// テキストの段落
struct TextParagraph {
    std::vector<TextPart> m_runs;
};

void UpdateTextRunSelection(TextRun& run);
INT HitTestTextRun(const TextRun& run, INT x, INT y);

bool ParseTextRun(TextRun& run, const std::wstring& text);

size_t DrawTextRun(
    HDC dc,
    TextRun& run,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags);
