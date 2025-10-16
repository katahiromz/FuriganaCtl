// base_textbox.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <unordered_map>

struct base_textbox_impl;
class base_textbox;

//////////////////////////////////////////////////////////////////////////////
// DECLARE_DYNAMIC/IMPLEMENT_DYNAMIC

#define DECLARE_DYNAMIC(class_name) \
    static base_textbox *create_instance();

#define IMPLEMENT_DYNAMIC(class_name) \
    /*static*/ base_textbox *class_name::create_instance() \
    { \
        return new(std::nothrow) class_name(); \
    } \
    struct class_name##AutoDynamicRegister \
    { \
        class_name##AutoDynamicRegister() \
        { \
            base_textbox::class_to_create_map()[TEXT(#class_name)] = &class_name::create_instance; \
        } \
    } class_name##AutoDynamicRegister##__LINE__;

class base_textbox;
using create_self_t = base_textbox *(*)(void);

//////////////////////////////////////////////////////////////////////////////
// base_textbox

class base_textbox
{
public:
    DECLARE_DYNAMIC(base_textbox);

    base_textbox(HWND hwnd = nullptr);
    virtual ~base_textbox();

    DWORD get_style() const;
    DWORD get_exstyle() const;
    static LPCWSTR get_class_name() { return L"base_textbox"; }
    static base_textbox *get_self(HWND hwnd) {
        return reinterpret_cast<base_textbox *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    LPCWSTR get_text() const;
    INT get_text_length() const;

    virtual LRESULT CALLBACK def_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static BOOL register_class(HINSTANCE inst);
    static BOOL unregister_class(HINSTANCE inst);

protected:
    friend struct base_textbox_impl;
    struct base_textbox_impl *m_pimpl = nullptr;

    virtual LRESULT CALLBACK window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void draw_client(HWND hwnd, HDC dc, RECT *client_rc);

public:
    // for DECLARE_DYNAMIC/IMPLEMENT_DYNAMIC
    typedef std::unordered_map<std::wstring, create_self_t> class_to_create_map_t;

    static class_to_create_map_t& class_to_create_map()
    {
        static class_to_create_map_t s_class_to_create_map;
        return s_class_to_create_map;
    }
};
