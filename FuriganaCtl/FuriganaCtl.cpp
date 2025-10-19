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

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl_impl

#include "FuriganaCtl_impl.h"

// FC_SETRUBYRATIO
LRESULT FuriganaCtl_impl::OnSetRubyRatio(INT mul, INT div) {
    if (mul >= 0 && div > 0)
    {
        m_ruby_ratio_mul = mul; // ルビ比率の分子
        m_ruby_ratio_div = div; // ルビ比率の分母
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
    lf.lfHeight *= m_ruby_ratio_mul;
    lf.lfHeight /= m_ruby_ratio_div;

    if (m_own_sub_font && m_sub_font)
        ::DeleteObject(m_sub_font);

    m_sub_font = ::CreateFontIndirect(&lf);

    BaseTextBox_impl::OnSetFont(hwndCtl, hfont, fRedraw);
}

INT FuriganaCtl_impl::HitTest(INT x, INT y) {
    x -= m_margin_rect.left;
    y -= m_margin_rect.top;
    return m_doc.HitTest(x, y);
}

// WM_LBUTTONDOWN
void FuriganaCtl_impl::OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags) {
    if (fDoubleClick)
        return;

    SetCapture(hwnd);

    INT iPart = HitTest(x, y);
    m_doc.m_selection_start = m_doc.m_selection_end = iPart;
    m_self->invalidate();
}

// WM_MOUSEMOVE
void FuriganaCtl_impl::OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags) {
    if (::GetCapture() != hwnd)
        return;

    INT iPart = HitTest(x, y);
    m_doc.m_selection_end = iPart;

    m_self->invalidate();
}

// WM_LBUTTONUP
void FuriganaCtl_impl::OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags) {
    if (::GetCapture() != hwnd)
        return;

    INT iPart = HitTest(x, y);
    m_doc.m_selection_end = iPart;

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
    doc.DrawDoc(dc, &rc, pimpl()->m_font, pimpl()->m_sub_font, flags, pimpl()->m_colors);
}

void FuriganaCtl::invalidate() {
    TextDoc& doc = pimpl()->m_doc;
    doc.Clear();
    doc.AddText(pimpl()->m_text);
    BaseTextBox::invalidate();
}

LRESULT CALLBACK FuriganaCtl::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, pimpl()->OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, pimpl()->OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, pimpl()->OnLButtonUp);
        HANDLE_MSG(hwnd, WM_SYSCOLORCHANGE, pimpl()->OnSysColorChange);
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
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        FuriganaCtl::register_class(NULL);
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        FuriganaCtl::unregister_class(NULL);
        break;
    }
    return TRUE;
}
#endif  // def FURIGANA_CTL_EXPORT
