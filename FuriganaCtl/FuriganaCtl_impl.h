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
    COLORREF m_colors[4];
    bool m_color_is_set[4];

    FuriganaCtl_impl(HWND hwnd, BaseTextBox *self) : BaseTextBox_impl(hwnd, self) {
        m_sub_font = NULL;
        m_own_sub_font = false;
        m_ruby_ratio_mul = 4; // ルビ比率の分子
        m_ruby_ratio_div = 5; // ルビ比率の分母
        SetRect(&m_margin_rect, 2, 2, 2, 2);

        m_colors[0] = GetSysColor(COLOR_WINDOWTEXT);
        m_colors[1] = GetSysColor(COLOR_WINDOW);
        m_colors[2] = GetSysColor(COLOR_HIGHLIGHTTEXT);
        m_colors[3] = GetSysColor(COLOR_HIGHLIGHT);
        m_color_is_set[0] = false;
        m_color_is_set[1] = false;
        m_color_is_set[2] = false;
        m_color_is_set[3] = false;
    }

    virtual void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
    virtual void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
    virtual void OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags);
    virtual void OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags);
    virtual void OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
    virtual void OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos);
    virtual UINT OnGetDlgCode(HWND hwnd, LPMSG lpmsg);
    virtual void OnSysColorChange(HWND hwnd);
    virtual void OnCopy(HWND hwnd);
    virtual void OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);

    virtual LRESULT OnSetRubyRatio(INT mul, INT div);
    virtual LRESULT OnSetMargin(LPRECT prc);
    virtual LRESULT OnSetColor(INT iColor, COLORREF rgbColor);
    virtual LRESULT OnSetLineGap(INT line_gap);
    virtual LRESULT OnSetSel(INT iStartSel, INT iEndSel);

    virtual INT hit_test(INT x, INT y);
    void SelectAll() {
        OnSetSel(0, -1);
    }
};
