// FuriganaCtl.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../BaseTextBox/BaseTextBox.h"

struct FuriganaCtl_impl;

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl

class FuriganaCtl : public BaseTextBox
{
public:
    DECLARE_DYNAMIC(FuriganaCtl);

    FuriganaCtl();
    virtual ~FuriganaCtl();

    static LPCWSTR get_class_name() { return L"FuriganaCtl"; }
    static FuriganaCtl *get_self(HWND hwnd) {
        return reinterpret_cast<FuriganaCtl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    FuriganaCtl_impl *pimpl() {
        return reinterpret_cast<FuriganaCtl_impl *>(BaseTextBox::m_pimpl);
    }
    const FuriganaCtl_impl *pimpl() const {
        return reinterpret_cast<FuriganaCtl_impl *>(BaseTextBox::m_pimpl);
    }

    static BOOL register_class(HINSTANCE inst);
    static BOOL unregister_class(HINSTANCE inst);

protected:
    friend struct FuriganaCtl_impl;

    virtual LRESULT CALLBACK window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
