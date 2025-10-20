// furigana_gdi.h
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include "pstdint.h"

struct TextDoc;

/////////////////////////////////////////////////////////////////////////////
// TextPart - テキスト パート

struct TextPart {
    enum Type {
        NORMAL, // 通常テキスト
        RUBY,   // ルビブロック
        NEWLINE // 改行文字
    } m_type;

    std::wstring m_text;

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
    bool has_ruby() const { return m_ruby_len > 0; }
    void update_width(TextDoc& doc);
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

    void update_width(TextDoc& doc);
    void update_height(TextDoc& doc);
};

/////////////////////////////////////////////////////////////////////////////
// TextPara - テキストの段落

struct TextPara {
    INT m_part_index_start;
    INT m_part_index_end;

    TextPara() {
        m_part_index_start = 0;
        m_part_index_end = 0;
    }
};

/////////////////////////////////////////////////////////////////////////////
// TextDoc - テキスト文書

struct TextDoc {
    std::wstring m_text;
    std::vector<TextPart> m_parts;
    std::vector<TextRun> m_runs;
    std::vector<TextPara> m_paras;
    HDC m_dc;
    INT m_base_height;
    INT m_ruby_height;
    INT m_selection_start; // パートのインデックス。
    INT m_selection_end; // パートのインデックス。
    INT m_para_width;
    INT m_para_height;
    INT m_max_width;
    INT m_line_gap;
    INT m_ruby_ratio_mul;
    INT m_ruby_ratio_div;
    HFONT m_hBaseFont;
    HFONT m_hRubyFont;
    bool m_layout_dirty;

    TextDoc() {
        m_dc = CreateCompatibleDC(NULL);
        m_base_height = 0;
        m_ruby_height = 0;
        m_selection_start = -1;
        m_selection_end = 0;
        m_para_width = 0;
        m_para_height = 0;
        m_max_width = 0;
        m_line_gap = 2;
        m_hBaseFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        m_hRubyFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        m_layout_dirty = true;
    }
    ~TextDoc() {
        DeleteDC(m_dc);
    }

    void add_text(const std::wstring& text);
    void clear();
    void set_selection(INT iStart, INT iEnd);
    std::wstring get_selection_text();
    void set_dirty();
    void set_fonts(HFONT hBaseFont, HFONT hRubyFont);
    void get_normalized_selection(INT& iStart, INT& iEnd);

    INT hit_test(INT x, INT y);

    void draw_doc(HDC dc, LPRECT prc, UINT flags, const COLORREF *colors = NULL);
    void get_ideal_size(LPRECT prc, UINT flags);
    INT update_runs();
    bool get_part_position(INT iPart, INT layout_width, LPPOINT ppt, UINT flags);
    INT get_part_height(INT iPart);

protected:
    void _update_parts_height();
    void _update_parts_width();

    void _draw_run(
        HDC dc,
        TextRun& run,
        LPRECT prc,
        UINT flags,
        const COLORREF *colors = NULL);
    void _add_para(const std::wstring& text);
};

/////////////////////////////////////////////////////////////////////////////
