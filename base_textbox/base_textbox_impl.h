// base_textbox_impl.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////////////
// base_textbox_impl_t

struct base_textbox_impl_t {
    base_textbox_t *m_self = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    HFONT m_font = nullptr;
    bool m_own_font = false;
    wchar_t *m_text = nullptr;
    INT m_text_length = 0;
    INT m_text_capacity = 0;

    base_textbox_impl_t(HWND hwnd, base_textbox_t *self) {
        m_self = self;
        m_hwnd = hwnd;
        m_hwndParent = hwnd ? ::GetParent(hwnd) : nullptr;
    }
    ~base_textbox_impl_t() {
        if (m_own_font && m_font)
            ::DeleteObject(m_font);
        delete[] m_text;
    }

    void paint_client(HWND hwnd, HDC hDC = nullptr);

    virtual BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
    virtual void OnDestroy(HWND hwnd);
    virtual HFONT OnGetFont(HWND hwnd);
    virtual void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
    virtual INT OnGetText(HWND hwnd, int cchTextMax, LPTSTR lpszText);
    virtual INT OnGetTextLength(HWND hwnd);
    virtual void OnSetText(HWND hwnd, LPCTSTR lpszText);
    virtual void OnPaint(HWND hwnd);
};
