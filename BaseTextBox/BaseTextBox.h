// BaseTextBox.h
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif
#include <map>
#include <string>

struct BaseTextBox_impl;
class BaseTextBox;

//////////////////////////////////////////////////////////////////////////////
// DECLARE_DYNAMIC/IMPLEMENT_DYNAMIC

#define DECLARE_DYNAMIC(class_name) \
    static BaseTextBox *create_instance();

#define IMPLEMENT_DYNAMIC(class_name) \
    /*static*/ BaseTextBox *class_name::create_instance() \
    { \
        return new(std::nothrow) class_name(); \
    } \
    struct class_name##AutoDynamicRegister \
    { \
        class_name##AutoDynamicRegister() \
        { \
            std::wstring cls_name = TEXT(#class_name); \
            CharUpperW(&cls_name[0]); \
            BaseTextBox::class_to_create_map()[cls_name.c_str()] = &class_name::create_instance; \
        } \
    } class_name##AutoDynamicRegister##__LINE__;

class BaseTextBox;
typedef BaseTextBox *(*create_self_t)(void);

//////////////////////////////////////////////////////////////////////////////
// BaseTextBox

class BaseTextBox
{
public:
    DECLARE_DYNAMIC(BaseTextBox);

    BaseTextBox(HWND hwnd = NULL);
    virtual ~BaseTextBox();

    DWORD get_style() const;
    DWORD get_exstyle() const;
    static LPCWSTR get_class_name() { return L"BaseTextBox"; }
    static BaseTextBox *get_self(HWND hwnd) {
        return reinterpret_cast<BaseTextBox *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    LPCWSTR get_text() const;
    INT get_text_length() const;

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static BOOL register_class(HINSTANCE inst);
    static BOOL unregister_class(HINSTANCE inst);

protected:
    friend struct BaseTextBox_impl;
    struct BaseTextBox_impl *m_pimpl;

    virtual LRESULT CALLBACK window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    // for DECLARE_DYNAMIC/IMPLEMENT_DYNAMIC
    typedef std::map<std::wstring, create_self_t> class_to_create_map_t;

    static class_to_create_map_t& class_to_create_map()
    {
        static class_to_create_map_t s_class_to_create_map;
        return s_class_to_create_map;
    }
};
