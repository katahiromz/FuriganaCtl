#pragma once

#include "../BaseTextBox/BaseTextBox_impl.h"
#include "../furigana_gdi/furigana_gdi.h"

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl_impl

struct FuriganaCtl_impl : BaseTextBox_impl {
    HFONT m_sub_font;
    bool m_own_sub_font;
    INT m_ruby_ratio_mul; // ルビ比率の分子
    INT m_ruby_ratio_div; // ルビ比率の分母
    INT m_delta_x;
    INT m_selection_start;
    INT m_selection_end;
    RECT m_margin_rect;
    std::vector<TextPart> m_parts;

    FuriganaCtl_impl(HWND hwnd, BaseTextBox *self) : BaseTextBox_impl(hwnd, self) {
        m_sub_font = NULL;
        m_own_sub_font = false;
        m_ruby_ratio_mul = 4; // ルビ比率の分子
        m_ruby_ratio_div = 5; // ルビ比率の分母
        m_delta_x = 0;
        m_selection_start = -1;
        m_selection_end = -1;
        SetRect(&m_margin_rect, 2, 2, 2, 2);
    }

    void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
};
