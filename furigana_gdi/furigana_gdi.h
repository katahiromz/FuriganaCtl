// furigana_gdi.h
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include "pstdint.h"

struct TextPara;

/////////////////////////////////////////////////////////////////////////////
// TextPart - テキスト パート

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

    void UpdateWidth(TextPara& para, HFONT hBaseFont, HFONT hRubyFont);
};


/////////////////////////////////////////////////////////////////////////////
// TextRun - テキストの連続

struct TextRun {
    INT m_part_index_start;
    INT m_part_index_end;
    INT m_base_height;
    INT m_ruby_height;
    INT m_run_width;
    INT m_run_height;
    INT m_max_width;
    INT m_delta_x;
    bool m_has_ruby;

    TextRun() {
        m_part_index_start = 0;
        m_part_index_end = 0;
        m_base_height = 0;
        m_ruby_height = 0;
        m_run_width = 0;
        m_run_height = 0;
        m_delta_x = 0;
        m_has_ruby = false;
    }

    void UpdateWidth(TextPara& para, HFONT hBaseFont, HFONT hRubyFont);
    void UpdateHeight(TextPara& para, HFONT hBaseFont, HFONT hRubyFont);

    size_t Draw(
        HDC dc,
        TextPara& para,
        LPRECT prc,
        HFONT hBaseFont,
        HFONT hRubyFont,
        UINT flags);
};

/////////////////////////////////////////////////////////////////////////////
// TextPara - テキストの段落

struct TextPara {
    std::wstring m_text;
    std::vector<TextPart> m_parts;
    std::vector<TextRun> m_runs;
    HDC m_dc;
    INT m_base_height;
    INT m_ruby_height;
    INT m_selection_start;
    INT m_selection_end;
    INT m_para_width;
    INT m_para_height;
    INT m_max_width;
    INT m_line_gap;

    TextPara() {
        m_dc = CreateCompatibleDC(NULL);
        m_base_height = 0;
        m_ruby_height = 0;
        m_selection_start = -1;
        m_selection_end = -1;
        m_para_width = 0;
        m_para_height = 0;
        m_max_width = 0;
        m_line_gap = 2;
    }
    ~TextPara() {
        DeleteDC(m_dc);
    }

    bool ParseParts(const std::wstring& text);
    INT PopulateRuns(HFONT hBaseFont, HFONT hRubyFont);
    void UpdateSelection();

    INT HitTest(INT x, INT y) const;

    void Update(TextPara& para, HFONT hBaseFont, HFONT hRubyFont);

    void DrawPara(
        HDC dc,
        LPRECT prc,
        HFONT hBaseFont,
        HFONT hRubyFont,
        UINT flags);

protected:
    void _UpdatePartsHeight(HFONT hBaseFont, HFONT hRubyFont);
    void _UpdatePartsWidth(HFONT hBaseFont, HFONT hRubyFont);

    void _DrawRun(
        HDC dc,
        TextRun& run,
        LPRECT prc,
        HFONT hBaseFont,
        HFONT hRubyFont,
        UINT flags);
};

/////////////////////////////////////////////////////////////////////////////
