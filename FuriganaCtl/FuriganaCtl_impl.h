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

        reset_colors();
    }

    void select_all() { OnSetSel(0, -1); }
    void reset_colors();
    UINT get_draw_flags() const;

    virtual INT hit_test(INT x, INT y);
    virtual void invalidate();

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
    virtual void OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys);

    virtual LRESULT OnSetRubyRatio(INT mul, INT div);
    virtual LRESULT OnSetMargin(LPRECT prc);
    virtual LRESULT OnSetColor(INT iColor, COLORREF rgbColor);
    virtual LRESULT OnSetLineGap(INT line_gap);
    virtual LRESULT OnSetSel(INT iStartSel, INT iEndSel);
};
