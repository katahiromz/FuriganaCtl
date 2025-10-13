// base_textbox.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "base_textbox.h"
#include <windowsx.h>
#include <new>
#include <cassert>

static inline void out_of_memory() {
    OutputDebugStringA("Out of memory!\n");
    MessageBoxA(nullptr, "Out of memory!", "Error", MB_ICONERROR);
}

//////////////////////////////////////////////////////////////////////////////
// base_textbox_impl_t

struct base_textbox_impl_t {
    base_textbox_t *m_self = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    HFONT m_font = nullptr;
    bool m_own_font = false;
    WNDPROC m_old_wndproc = nullptr;
    wchar_t *m_text_cache = nullptr;
    INT m_text_cache_capacity = 0;

    base_textbox_impl_t(HWND hwnd, base_textbox_t *self) {
        m_self = self;
        m_hwnd = hwnd;
        m_hwndParent = hwnd ? ::GetParent(hwnd) : nullptr;
    }
    ~base_textbox_impl_t() {
        if (m_own_font && m_font)
            ::DeleteObject(m_font);
        delete[] m_text_cache;
    }

    BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
    void OnDestroy(HWND hwnd);
    HFONT OnGetFont(HWND hwnd);
    void OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw);
    void OnPaint(HWND hwnd);
    void paint_client(HWND hwnd, HDC hDC = nullptr);
};

// WM_CREATE
BOOL base_textbox_impl_t::OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct) {
    m_hwnd = hwnd;
    m_hwndParent = ::GetParent(hwnd);

    LOGFONTW lf;
    HGDIOBJ hobj = ::GetStockObject(DEFAULT_GUI_FONT);
    if (hobj) {
        ::GetObjectW(hobj, sizeof(lf), &lf);
        m_font = CreateFontIndirectW(&lf);
        m_own_font = true;
    } else {
        m_font = nullptr;
        m_own_font = false;
    }

    return TRUE;
}

// WM_DESTROY
void base_textbox_impl_t::OnDestroy(HWND hwnd) {
    if (m_own_font) {
        ::DeleteObject(m_font);
        m_font = nullptr;
        m_own_font = false;
    }
}

// WM_GETFONT
HFONT base_textbox_impl_t::OnGetFont(HWND hwnd) {
    return m_font;
}

// WM_SETFONT
void base_textbox_impl_t::OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw) {
    if (m_font == hfont)
        return;

    if (m_font)
        ::DeleteObject(m_font);
    m_font = hfont;
    m_own_font = false;

    if (fRedraw)
        ::InvalidateRect(hwndCtl, nullptr, TRUE);
}

// WM_PAINT
void base_textbox_impl_t::OnPaint(HWND hwnd) {
    paint_client(hwnd, nullptr);
}

// WM_PAINT, WM_PRINTCLIENT
void base_textbox_impl_t::paint_client(HWND hwnd, HDC hDC) {
    if (!::IsWindowVisible(hwnd))
        return;

    PAINTSTRUCT ps;
    HDC dc = hDC ? hDC : ::BeginPaint(hwnd, &ps);
    if (!dc)
        return;

    RECT client_rect;
    ::GetClientRect(hwnd, &client_rect);

    HGDIOBJ old_font = nullptr;
    if (m_font)
        old_font = ::SelectObject(dc, m_font);

    m_self->draw_client(hwnd, dc, &client_rect);

    if (m_font && old_font)
        ::SelectObject(dc, old_font);

    if (!hDC)
        ::EndPaint(hwnd, &ps);
}

//////////////////////////////////////////////////////////////////////////////
// base_textbox_t

base_textbox_t::base_textbox_t(HWND hwnd) {
    m_pimpl = new base_textbox_impl_t(hwnd, this);
    if (!m_pimpl) {
        out_of_memory();
    }
}

base_textbox_t::~base_textbox_t() {
    delete m_pimpl;
}

DWORD base_textbox_t::get_style() const {
    return GetWindowStyle(m_pimpl->m_hwnd);
}

DWORD base_textbox_t::get_exstyle() const {
    return GetWindowExStyle(m_pimpl->m_hwnd);
}

BOOL base_textbox_t::register_class(HINSTANCE inst, LPCWSTR class_name, WNDPROC wndproc) {
    WNDCLASSEXW wcx = { sizeof(wcx) };
    wcx.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = wndproc;
    wcx.hInstance = inst;
    wcx.hIcon = NULL;
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = NULL; // optimized
    wcx.lpszClassName = class_name;
    wcx.hIconSm = NULL;
    return ::RegisterClassExW(&wcx);
}

BOOL base_textbox_t::unregister_class(HINSTANCE inst, LPCWSTR class_name) {
    return ::UnregisterClassW(class_name, inst);
}

void base_textbox_t::subclass(HWND hwnd, WNDPROC new_wndproc) {
    assert(!m_pimpl->m_hwnd);
    assert(new_wndproc);
    m_pimpl->m_hwnd = hwnd;
    m_pimpl->m_old_wndproc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)new_wndproc);
    assert(m_pimpl->m_old_wndproc);
}

void base_textbox_t::unsubclass() {
    if (!m_pimpl->m_hwnd)
        return;
    SetWindowLongPtrW(m_pimpl->m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_pimpl->m_old_wndproc);
    m_pimpl->m_hwnd = nullptr;
}

LRESULT CALLBACK
base_textbox_t::def_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (m_pimpl->m_old_wndproc)
        return ::CallWindowProcW(m_pimpl->m_old_wndproc, hwnd, uMsg, wParam, lParam);
    else
        return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK
base_textbox_t::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    base_textbox_t *self = get_self(hwnd);
    if (!self) {
        if (uMsg == WM_NCCREATE) {
            self = new(std::nothrow) base_textbox_t(hwnd);
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
        if (self->m_pimpl->m_old_wndproc) {
            self->unsubclass();
        } else {
            delete self;
        }
    }

    return ret;
}

LRESULT CALLBACK
base_textbox_t::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_CREATE, m_pimpl->OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, m_pimpl->OnDestroy);
        HANDLE_MSG(hwnd, WM_PAINT, m_pimpl->OnPaint);
        HANDLE_MSG(hwnd, WM_GETFONT, m_pimpl->OnGetFont);
        HANDLE_MSG(hwnd, WM_SETFONT, m_pimpl->OnSetFont);
    case WM_ERASEBKGND:
        return TRUE; // Done in m_pimpl->OnPaint
    case WM_PRINTCLIENT:
        m_pimpl->paint_client(hwnd, (HDC)wParam);
        break;
    default:
        return def_window_proc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void base_textbox_t::draw_client(HWND hwnd, HDC dc, RECT *client_rc) {
    assert(client_rc);

    ::FillRect(dc, client_rc, ::GetSysColorBrush(COLOR_WINDOW));

    INT text_len = ::GetWindowTextLengthW(hwnd);
    INT required_capacity = text_len + 1;

    if (m_pimpl->m_text_cache_capacity >= 8 * required_capacity) {
        delete[] m_pimpl->m_text_cache;
        m_pimpl->m_text_cache = new(std::nothrow) wchar_t[required_capacity];
        m_pimpl->m_text_cache_capacity = required_capacity;
    }

    if (required_capacity > m_pimpl->m_text_cache_capacity) {
        delete[] m_pimpl->m_text_cache;
        m_pimpl->m_text_cache = new(std::nothrow) wchar_t[required_capacity];
        if (!m_pimpl->m_text_cache) {
            out_of_memory();
            return;
        }
        m_pimpl->m_text_cache_capacity = required_capacity;
    }

    wchar_t *text = m_pimpl->m_text_cache;
    ::GetWindowTextW(hwnd, text, required_capacity);

    INT old_mode = ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    ::DrawText(dc, text, -1, client_rc, DT_LEFT | DT_TOP | DT_EDITCONTROL | DT_EXPANDTABS);
    ::SetBkMode(dc, old_mode);
}

//////////////////////////////////////////////////////////////////////////////
// DllMain

#ifdef BASE_TEXTBOX_EXPORT
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        base_textbox_t::register_class(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        base_textbox_t::unregister_class(hinstDLL);
        break;
    }
    return TRUE;
}
#endif  // def BASE_TEXTBOX_EXPORT
