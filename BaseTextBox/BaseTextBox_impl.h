// BaseTextBox_impl.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////////////
// BaseTextBox_impl

struct BaseTextBox_impl {
    BaseTextBox *m_self;
    HWND m_hwnd;
    HWND m_hwndParent;
    HFONT m_font;
    bool m_own_font;
    std::wstring m_text;

    BaseTextBox_impl(BaseTextBox *self) {
        m_self = self;
        m_hwnd = NULL;
        m_hwndParent = NULL;
        m_font = NULL;
        m_own_font = false;
    }
    virtual ~BaseTextBox_impl() {
        if (m_own_font && m_font)
            ::DeleteObject(m_font);
    }

    void paint_client(HWND hwnd, HDC hDC = NULL);
    virtual void paint_inner(HWND hwnd, HDC dc, RECT *rect);
    virtual void invalidate();

    virtual BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
    virtual void OnDestroy(HWND hwnd);
    virtual HFONT OnGetFont(HWND hwnd);
    virtual void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
    virtual INT OnGetText(HWND hwnd, INT cchTextMax, LPTSTR lpszText);
    virtual INT OnGetTextLength(HWND hwnd);
    virtual void OnSetText(HWND hwnd, LPCTSTR lpszText);
    virtual void OnPaint(HWND hwnd);
};
