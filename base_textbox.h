// base_textbox.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

struct base_textbox_impl_t;

//////////////////////////////////////////////////////////////////////////////
// base_textbox_t

class base_textbox_t
{
public:
    base_textbox_t(HWND hwnd = nullptr);
    virtual ~base_textbox_t();

    DWORD get_style() const;
    DWORD get_exstyle() const;
    static LPCWSTR get_class_name() { return L"base_textbox"; }
    static base_textbox_t *get_self(HWND hwnd) {
        return reinterpret_cast<base_textbox_t *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    virtual LRESULT CALLBACK def_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static BOOL register_class(HINSTANCE inst, LPCWSTR class_name = base_textbox_t::get_class_name(), WNDPROC wndproc = base_textbox_t::window_proc);
    static BOOL unregister_class(HINSTANCE inst, LPCWSTR class_name = base_textbox_t::get_class_name());
    void subclass(HWND hwnd, WNDPROC new_wndproc = base_textbox_t::window_proc);
    void unsubclass();

protected:
    friend struct base_textbox_impl_t;
    struct base_textbox_impl_t *m_pimpl = nullptr;

    virtual LRESULT CALLBACK window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void draw_client(HWND hwnd, HDC dc, RECT *client_rc);
};
