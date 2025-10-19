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
    RECT m_margin_rect;
    TextDoc m_doc;

    FuriganaCtl_impl(HWND hwnd, BaseTextBox *self) : BaseTextBox_impl(hwnd, self) {
        m_sub_font = NULL;
        m_own_sub_font = false;
        m_ruby_ratio_mul = 4; // ルビ比率の分子
        m_ruby_ratio_div = 5; // ルビ比率の分母
        SetRect(&m_margin_rect, 2, 2, 2, 2);
    }

    virtual void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
    virtual void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
    virtual void OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags);
    virtual void OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags);

    virtual INT HitTest(INT x, INT y);
};
