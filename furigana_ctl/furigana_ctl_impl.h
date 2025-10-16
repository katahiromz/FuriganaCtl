#pragma once

#include "../base_textbox/base_textbox_impl.h"

//////////////////////////////////////////////////////////////////////////////
// furigana_ctl_impl

struct furigana_ctl_impl : base_textbox_impl {
    furigana_ctl_impl(HWND hwnd, base_textbox *self) : base_textbox_impl(hwnd, self) {}

    HFONT m_sub_font = nullptr;
    bool m_own_sub_font = false;
    INT m_ruby_ratio_mul = 4; // ルビ比率の分子
    INT m_ruby_ratio_div = 5; // ルビ比率の分母

    void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw) override;
};
