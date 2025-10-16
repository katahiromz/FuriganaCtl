// furigana_ctl.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../base_textbox/base_textbox.h"

struct furigana_ctl_impl;

//////////////////////////////////////////////////////////////////////////////
// furigana_ctl

class furigana_ctl : public base_textbox
{
public:
    DECLARE_DYNAMIC(furigana_ctl);

    furigana_ctl(HWND hwnd = NULL);
    virtual ~furigana_ctl();

    static LPCWSTR get_class_name() { return L"furigana_ctl"; }
    static furigana_ctl *get_self(HWND hwnd) {
        return reinterpret_cast<furigana_ctl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    furigana_ctl_impl *pimpl() { return reinterpret_cast<furigana_ctl_impl *>(base_textbox::m_pimpl); }

    static BOOL register_class(HINSTANCE inst);
    static BOOL unregister_class(HINSTANCE inst);

protected:
    friend struct furigana_ctl_impl;

    virtual LRESULT CALLBACK window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void draw_client(HWND hwnd, HDC dc, RECT *client_rc) override;
};
