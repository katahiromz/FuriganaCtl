#pragma once

#include "../base_textbox/base_textbox_impl.h"

//////////////////////////////////////////////////////////////////////////////
// furigana_ctl_impl

struct furigana_ctl_impl : base_textbox_impl {
    HFONT m_sub_font;
    bool m_own_sub_font;
    INT m_ruby_ratio_mul; // ルビ比率の分子
    INT m_ruby_ratio_div; // ルビ比率の分母

    furigana_ctl_impl(HWND hwnd, base_textbox *self) : base_textbox_impl(hwnd, self) {
        m_sub_font = NULL;
        m_own_sub_font = false;
        m_ruby_ratio_mul = 4; // ルビ比率の分子
        m_ruby_ratio_div = 5; // ルビ比率の分母
    }

    void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
};
