// BaseTextBox.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "BaseTextBox.h"
#include <windowsx.h>
#include <new>
#include <cassert>

static inline void out_of_memory() {
    OutputDebugStringA("Out of memory!\n");
    MessageBoxA(NULL, "Out of memory!", "Error", MB_ICONERROR);
}

//////////////////////////////////////////////////////////////////////////////
// BaseTextBox_impl

#include "BaseTextBox_impl.h"

void BaseTextBox_impl::invalidate() {
    ::InvalidateRect(m_hwnd, NULL, FALSE);
}

// WM_CREATE
BOOL BaseTextBox_impl::OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct) {
    m_hwnd = hwnd;
    m_hwndParent = ::GetParent(hwnd);

    LOGFONTW lf;
    HGDIOBJ hobj = ::GetStockObject(DEFAULT_GUI_FONT);
    if (hobj) {
        ::GetObjectW(hobj, sizeof(lf), &lf);
        m_font = CreateFontIndirectW(&lf);
        m_own_font = true;
    } else {
        m_font = NULL;
        m_own_font = false;
    }

    OnSetText(hwnd, lpCreateStruct->lpszName);

    return TRUE;
}

// WM_DESTROY
void BaseTextBox_impl::OnDestroy(HWND hwnd) {
    if (m_own_font) {
        ::DeleteObject(m_font);
        m_font = NULL;
        m_own_font = false;
    }
}

// WM_GETFONT
HFONT BaseTextBox_impl::OnGetFont(HWND hwnd) {
    return m_font;
}

// WM_SETFONT
void BaseTextBox_impl::OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw) {
    if (m_font == hfont)
        return;

    if (m_own_font && m_font)
        ::DeleteObject(m_font);

    m_font = hfont;
    m_own_font = false;

    if (fRedraw)
        invalidate();
}

// WM_GETTEXT
INT BaseTextBox_impl::OnGetText(HWND hwnd, INT cchTextMax, LPTSTR lpszText) {
    if (!m_text) {
        if (cchTextMax > 0) lpszText[0] = 0;
        return 0;
    }
    lstrcpynW(lpszText, m_text, cchTextMax);
    return lstrlenW(lpszText);
}

// WM_GETTEXTLENGTH
INT BaseTextBox_impl::OnGetTextLength(HWND hwnd) {
    return lstrlenW(m_text);
}

// WM_SETTEXT
void BaseTextBox_impl::OnSetText(HWND hwnd, LPCTSTR lpszText) {
    if (!lpszText) lpszText = L"";

    INT text_len = lstrlenW(lpszText);
    INT required_capacity = text_len + 1;

    if (m_text_capacity >= 8 * required_capacity) {
        delete[] m_text;
        m_text = new(std::nothrow) wchar_t[required_capacity];
        if (!m_text) {
            out_of_memory();
            m_text_length = m_text_capacity = 0;
            return;
        }
        m_text_capacity = required_capacity;
    } else if (required_capacity > m_text_capacity) {
        delete[] m_text;
        const INT extra = 1024;
        m_text = new(std::nothrow) wchar_t[required_capacity + extra];
        if (!m_text) {
            out_of_memory();
            m_text_length = m_text_capacity = 0;
            return;
        }
        m_text_capacity = required_capacity + extra;
    }

    lstrcpynW(m_text, lpszText, required_capacity);
    m_text_length = text_len;
    invalidate();
}

void BaseTextBox_impl::paint_inner(HWND hwnd, HDC dc, RECT *rect) {
    HGDIOBJ old_font = NULL;
    if (m_font)
        old_font = ::SelectObject(dc, m_font);

    INT old_mode = ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
    ::DrawText(dc, m_text, m_text_length, rect,
               DT_LEFT | DT_TOP | DT_EXPANDTABS | DT_WORDBREAK);
    ::SetBkMode(dc, old_mode);

    if (m_font && old_font)
        ::SelectObject(dc, old_font);
}

// WM_PAINT, WM_PRINTCLIENT
void BaseTextBox_impl::paint_client(HWND hwnd, HDC hDC) {
    if (!::IsWindowVisible(hwnd))
        return;

    PAINTSTRUCT ps;
    HDC dc = hDC ? hDC : ::BeginPaint(hwnd, &ps);
    if (!dc)
        return;

    RECT rect;
    ::GetClientRect(hwnd, &rect);

    paint_inner(hwnd, dc, &rect);

    if (!hDC)
        ::EndPaint(hwnd, &ps);
}

// WM_PAINT
void BaseTextBox_impl::OnPaint(HWND hwnd) {
    paint_client(hwnd, NULL);    
}

//////////////////////////////////////////////////////////////////////////////
// BaseTextBox

IMPLEMENT_DYNAMIC(BaseTextBox);

BaseTextBox::BaseTextBox(HWND hwnd) {
    m_pimpl = new BaseTextBox_impl(hwnd, this);
    if (!m_pimpl) {
        out_of_memory();
    }
}

BaseTextBox::~BaseTextBox() {
    delete m_pimpl;
}

DWORD BaseTextBox::get_style() const {
    return (DWORD)::GetWindowLongPtrW(m_pimpl->m_hwnd, GWL_STYLE);
}

DWORD BaseTextBox::get_exstyle() const {
    return (DWORD)::GetWindowLongPtrW(m_pimpl->m_hwnd, GWL_EXSTYLE);
}

BOOL BaseTextBox::register_class(HINSTANCE inst) {
    WNDCLASSEXW wcx = { sizeof(wcx) };
    wcx.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = window_proc;
    wcx.hInstance = inst;
    wcx.hIcon = NULL;
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = NULL; // optimized
    wcx.lpszClassName = get_class_name();
    wcx.hIconSm = NULL;
    return ::RegisterClassExW(&wcx);
}

BOOL BaseTextBox::unregister_class(HINSTANCE inst) {
    return ::UnregisterClassW(get_class_name(), inst);
}

LRESULT CALLBACK
BaseTextBox::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    BaseTextBox *self = get_self(hwnd);
    if (!self) {
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
            std::wstring class_name = cs->lpszClass;
            ::CharUpperW(&class_name[0]);
            create_self_t create_self = (*class_to_create_map)()[class_name];
            assert(create_self);
            self = create_self();
            if (!self || !self->m_pimpl) {
                out_of_memory();
                return FALSE;
            }
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        } else {
            return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }

    LRESULT ret = self->window_proc_inner(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete self;
    }

    return ret;
}

LRESULT CALLBACK
BaseTextBox::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_CREATE, m_pimpl->OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, m_pimpl->OnDestroy);
        HANDLE_MSG(hwnd, WM_PAINT, m_pimpl->OnPaint);
        HANDLE_MSG(hwnd, WM_GETTEXT, m_pimpl->OnGetText);
        HANDLE_MSG(hwnd, WM_GETTEXTLENGTH, m_pimpl->OnGetTextLength);
        HANDLE_MSG(hwnd, WM_SETTEXT, m_pimpl->OnSetText);
        HANDLE_MSG(hwnd, WM_GETFONT, m_pimpl->OnGetFont);
        HANDLE_MSG(hwnd, WM_SETFONT, m_pimpl->OnSetFont);
    case WM_ERASEBKGND:
        return TRUE; // Done in m_pimpl->OnPaint
    case WM_PRINTCLIENT:
        m_pimpl->paint_client(hwnd, (HDC)wParam);
        break;
    default:
        return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LPCWSTR BaseTextBox::get_text() const {
    return m_pimpl->m_text ? m_pimpl->m_text : L"";
}

INT BaseTextBox::get_text_length() const {
    return m_pimpl->m_text_length;
}

//////////////////////////////////////////////////////////////////////////////
// DllMain

#ifdef BASE_TEXTBOX_EXPORT
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        BaseTextBox::register_class(NULL);
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        BaseTextBox::unregister_class(NULL);
        break;
    }
    return TRUE;
}
#endif  // def BASE_TEXTBOX_EXPORT
