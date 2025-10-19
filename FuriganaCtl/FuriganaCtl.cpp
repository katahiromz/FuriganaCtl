// FuriganaCtl.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "FuriganaCtl.h"
#include "furigana_api.h"
#include "../furigana_gdi/furigana_gdi.h"
#include <windowsx.h>
#include <new>
#include <cassert>

static inline void out_of_memory() {
    OutputDebugStringA("Out of memory!\n");
    MessageBoxA(NULL, "Out of memory!", "Error", MB_ICONERROR);
}

static inline void DPRINTF(LPCWSTR fmt, ...) {
#ifndef NDEBUG
    WCHAR text[1024];
    va_list va;
    va_start(va, fmt);
    wvsprintfW(text, fmt, va);
    OutputDebugStringW(text);
    va_end(va);
#endif
}

static HINSTANCE s_hinstDLL = NULL;

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl_impl

#include "FuriganaCtl_impl.h"

// FC_SETRUBYRATIO
LRESULT FuriganaCtl_impl::OnSetRubyRatio(INT mul, INT div) {
    if (mul >= 0 && div > 0)
    {
        m_doc.m_ruby_ratio_mul = mul; // ルビ比率の分子
        m_doc.m_ruby_ratio_div = div; // ルビ比率の分母
        return TRUE;
    }
    return FALSE;
}

// FC_SETMARGIN
LRESULT FuriganaCtl_impl::OnSetMargin(LPRECT prc) {
    if (!prc) {
        SetRect(&m_margin_rect, 2, 2, 2, 2);
        return TRUE;
    }
    m_margin_rect = *prc;
    return TRUE;
}

// FC_SETCOLOR
LRESULT FuriganaCtl_impl::OnSetColor(INT iColor, COLORREF rgbColor) {
    if (iColor < 0 || iColor >= 4)
        return FALSE;

    if (rgbColor == CLR_INVALID) {
        COLORREF rgbDefault;
        switch (iColor) {
        case 0: rgbDefault = GetSysColor(COLOR_WINDOWTEXT); break;
        case 1: rgbDefault = GetSysColor(COLOR_WINDOW); break;
        case 2: rgbDefault = GetSysColor(COLOR_HIGHLIGHTTEXT); break;
        case 3: rgbDefault = GetSysColor(COLOR_HIGHLIGHT); break;
        default:
            assert(0);
        }
        m_colors[iColor] = rgbDefault;
        m_color_is_set[iColor] = false;
        return TRUE;
    }

    m_colors[iColor] = rgbColor;
    m_color_is_set[iColor] = true;
    m_self->invalidate();
    return TRUE;
}

// FC_SETLINEGAP
LRESULT FuriganaCtl_impl::OnSetLineGap(INT line_gap) {
    if (line_gap < 0)
        return FALSE;
    m_doc.m_line_gap = line_gap;
    m_self->invalidate();
    return TRUE;
}

// WM_SETFONT
void FuriganaCtl_impl::OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw) {
    if (!hfont)
        return;

    LOGFONT lf;
    ::GetObject(hfont, sizeof(lf), &lf);
    lf.lfHeight *= m_doc.m_ruby_ratio_mul;
    lf.lfHeight /= m_doc.m_ruby_ratio_div;

    if (m_own_sub_font && m_sub_font)
        ::DeleteObject(m_sub_font);

    m_sub_font = ::CreateFontIndirect(&lf);

    BaseTextBox_impl::OnSetFont(hwndCtl, hfont, fRedraw);

    m_doc.m_hBaseFont = m_font;
    m_doc.m_hRubyFont = m_sub_font;
}

// WM_KEYDOWN, WM_KEYUP
void FuriganaCtl_impl::OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    if (!fDown)
        return;

    if (GetKeyState(VK_CONTROL) < 0 && vk == 'C') { // Ctrl+C
        OnCopy(hwnd);
        return;
    }
    if (GetKeyState(VK_CONTROL) < 0 && vk == 'A') { // Ctrl+A
        SelectAll();
        return;
    }
}

// WM_GETDLGCODE
UINT FuriganaCtl_impl::OnGetDlgCode(HWND hwnd, LPMSG lpmsg) {
    return DLGC_WANTALLKEYS;
}

// WM_RBUTTONDOWN
void FuriganaCtl_impl::OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags) {
    SetFocus(hwnd);
}

LRESULT FuriganaCtl_impl::OnSetSel(INT iStartSel, INT iEndSel) {
    m_doc.m_selection_start = iStartSel;
    m_doc.m_selection_end = iEndSel;
    m_doc.UpdateSelection();
    m_self->invalidate();
    return TRUE;
}

// WM_CONTEXTMENU
void FuriganaCtl_impl::OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos) {
    if (xPos == 0xFFFF && yPos == 0xFFFF) {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        xPos = rc.left;
        yPos = rc.top;
    }

    HMENU hMenu = ::LoadMenu(s_hinstDLL, MAKEINTRESOURCE(1));
    HMENU hSubMenu = ::GetSubMenu(hMenu, 0);
    if (hSubMenu) {
        UINT uFlags = TPM_RIGHTBUTTON | TPM_RETURNCMD;
        INT id = TrackPopupMenu(hSubMenu, uFlags, xPos, yPos, 0, hwnd, NULL);
        if (id != 0 && id != -1) {
            switch (id) {
            case 32766:
                SelectAll();
                break;
            case 32767:
                OnCopy(hwnd);
                break;
            }
        }
    }
    ::DestroyMenu(hMenu);
}

// WM_COPY
void FuriganaCtl_impl::OnCopy(HWND hwnd) {
    std::wstring text = m_doc.GetSelectedText();

    BOOL ok = FALSE;
    if (::OpenClipboard(hwnd)) {
        EmptyClipboard();

        size_t cbText = (text.length() + 1) * sizeof(WCHAR);
        HGLOBAL hGlobal = ::GlobalAlloc(GHND | GMEM_SHARE, cbText);
        if (hGlobal) {
            LPWSTR pszText = (LPWSTR)::GlobalLock(hGlobal);
            ::CopyMemory(pszText, text.c_str(), cbText);
            ::GlobalUnlock(hGlobal);

            ::SetClipboardData(CF_UNICODETEXT, hGlobal);

            ok = TRUE;
        }

        ::CloseClipboard();
    }

    if (ok) {
        // TODO:
    } else {
        // TODO:
    }
}

INT FuriganaCtl_impl::HitTest(INT x, INT y) {
    x -= m_margin_rect.left;
    y -= m_margin_rect.top;
    return m_doc.HitTest(x, y);
}

// WM_LBUTTONDOWN
void FuriganaCtl_impl::OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags) {
    DPRINTF(L"OnLButtonDown: %d, %d, %p\n", x, y, GetCapture());
    SetFocus(hwnd);
    SetCapture(hwnd);

    INT iPart = HitTest(x, y);
    m_doc.SetSelection(iPart, iPart);
    m_self->invalidate();
}

// WM_MOUSEMOVE
void FuriganaCtl_impl::OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags) {
    //DPRINTF(L"OnMouseMove: %d, %d, %p\n", x, y, GetCapture());
    if (::GetCapture() != hwnd) {
        return;
    }

    INT iPart = HitTest(x, y);
    m_doc.SetSelection(m_doc.m_selection_start, iPart);

    m_self->invalidate();
}

// WM_LBUTTONUP
void FuriganaCtl_impl::OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags) {
    DPRINTF(L"OnLButtonUp: %d, %d, %p\n", x, y, GetCapture());
    if (::GetCapture() != hwnd) {
        return;
    }

    INT iPart = HitTest(x, y);
    m_doc.SetSelection(m_doc.m_selection_start, iPart);

    ::ReleaseCapture();
    m_self->invalidate();
}

// WM_SYSCOLORCHANGE
void FuriganaCtl_impl::OnSysColorChange(HWND hwnd) {
    INT iColor = 0;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID);
    m_self->invalidate();
}

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl

IMPLEMENT_DYNAMIC(FuriganaCtl);

FuriganaCtl::FuriganaCtl(HWND hwnd) {
    delete m_pimpl;
    m_pimpl = new FuriganaCtl_impl(hwnd, this);
    if (!m_pimpl) {
        out_of_memory();
    }
}

FuriganaCtl::~FuriganaCtl() { }

BOOL FuriganaCtl::register_class(HINSTANCE inst) {
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

BOOL FuriganaCtl::unregister_class(HINSTANCE inst) {
    return ::UnregisterClassW(get_class_name(), inst);
}

void FuriganaCtl::draw_client(HWND hwnd, HDC dc, RECT *client_rc) {
    HBRUSH hBrush = ::CreateSolidBrush(pimpl()->m_colors[1]);
    ::FillRect(dc, client_rc, hBrush);
    ::DeleteObject(hBrush);

    const RECT& margin_rect = pimpl()->m_margin_rect;
    RECT rc = *client_rc;
    rc.left += margin_rect.left;
    rc.top += margin_rect.top;
    rc.right -= margin_rect.right;
    rc.bottom -= margin_rect.bottom;
    IntersectClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);

    DWORD style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    DWORD flags = 0;
    if (!(style & ES_MULTILINE)) flags |= DT_SINGLELINE;
    if (style & ES_CENTER) flags |= DT_CENTER;
    if (style & ES_RIGHT) flags |= DT_RIGHT;

    TextDoc& doc = pimpl()->m_doc;
    doc.UpdateSelection();
    doc.DrawDoc(dc, &rc, flags, pimpl()->m_colors);
}

void FuriganaCtl::invalidate() {
    TextDoc& doc = pimpl()->m_doc;
    if (doc.m_text != pimpl()->m_text) {
        doc.clear();
        doc.AddText(pimpl()->m_text);
    }
    BaseTextBox::invalidate();
}

LRESULT CALLBACK FuriganaCtl::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, pimpl()->OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, pimpl()->OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, pimpl()->OnLButtonUp);
        HANDLE_MSG(hwnd, WM_RBUTTONDOWN, pimpl()->OnRButtonDown);
        HANDLE_MSG(hwnd, WM_SYSCOLORCHANGE, pimpl()->OnSysColorChange);
        HANDLE_MSG(hwnd, WM_COPY, pimpl()->OnCopy);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, pimpl()->OnContextMenu);
        HANDLE_MSG(hwnd, WM_GETDLGCODE, pimpl()->OnGetDlgCode);
        HANDLE_MSG(hwnd, WM_KEYDOWN, pimpl()->OnKey);
    case FC_SETRUBYRATIO:
        return pimpl()->OnSetRubyRatio((INT)wParam, (INT)lParam);
    case FC_SETMARGIN:
        return pimpl()->OnSetMargin((RECT *)lParam);
    case FC_SETCOLOR:
        return pimpl()->OnSetColor((INT)wParam, (COLORREF)lParam);
    case FC_SETLINEGAP:
        return pimpl()->OnSetLineGap((INT)wParam);
    default:
        return BaseTextBox::window_proc_inner(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// DllMain

#ifdef FURIGANA_CTL_EXPORT
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        s_hinstDLL = hinstDLL;
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        FuriganaCtl::register_class(NULL);
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        FuriganaCtl::unregister_class(NULL);
        s_hinstDLL = NULL;
        break;
    }
    return TRUE;
}
#endif  // def FURIGANA_CTL_EXPORT
