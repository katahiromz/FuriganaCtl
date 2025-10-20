// BaseTextBox.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "BaseTextBox.h"
#include <windowsx.h>
#include <new>
#include <cassert>

//////////////////////////////////////////////////////////////////////////////
// BaseTextBox_impl

#include "BaseTextBox_impl.h"

void BaseTextBox_impl::invalidate() {
    assert(m_hwnd);
    ::InvalidateRect(m_hwnd, NULL, FALSE);
}

// WM_CREATE
BOOL BaseTextBox_impl::OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct) {
    assert(hwnd == m_hwnd);
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
    lstrcpynW(lpszText, m_text.c_str(), cchTextMax);
    return lstrlenW(lpszText);
}

// WM_GETTEXTLENGTH
INT BaseTextBox_impl::OnGetTextLength(HWND hwnd) {
    return (INT)m_text.size();
}

// WM_SETTEXT
void BaseTextBox_impl::OnSetText(HWND hwnd, LPCTSTR lpszText) {
    if (!lpszText) lpszText = L"";

    m_text = lpszText;
    invalidate();
}

void BaseTextBox_impl::paint_inner(HWND hwnd, HDC dc, RECT *rect) {
    HGDIOBJ old_font = NULL;
    if (m_font)
        old_font = ::SelectObject(dc, m_font);

    if (m_text.size()) {
        INT old_mode = ::SetBkMode(dc, TRANSPARENT);
        ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
        ::DrawText(dc, m_text.c_str(), (INT)m_text.length(), rect,
                   DT_LEFT | DT_TOP | DT_EXPANDTABS | DT_WORDBREAK);
        ::SetBkMode(dc, old_mode);
    }

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

BaseTextBox::BaseTextBox() {
    m_pimpl = new(std::nothrow) BaseTextBox_impl(this);
    if (!m_pimpl) {
        OutputDebugStringA("Out of memory\n");
        assert(0);
    }
}

BaseTextBox::~BaseTextBox() {
    delete m_pimpl;
}

DWORD BaseTextBox::get_style() const {
    assert(m_pimpl && m_pimpl->m_hwnd);
    return (DWORD)::GetWindowLongPtrW(m_pimpl->m_hwnd, GWL_STYLE);
}

DWORD BaseTextBox::get_exstyle() const {
    assert(m_pimpl && m_pimpl->m_hwnd);
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
                OutputDebugStringA("Failed in BaseTextBox::window_proc\n");
                return FALSE;
            }
            self->m_pimpl->m_hwnd = hwnd;
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
    return m_pimpl->m_text.c_str();
}

INT BaseTextBox::get_text_length() const {
    return (INT)m_pimpl->m_text.length();
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
