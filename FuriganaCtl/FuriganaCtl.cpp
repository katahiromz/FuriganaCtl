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

INT FuriganaCtl::HitTest(INT x, INT y) {
    return HitTestTextPart(pimpl()->m_parts, x - pimpl()->m_delta_x, y);
}

void FuriganaCtl::OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags) {
    if (fDoubleClick)
        return;

    SetCapture(hwnd);
    INT iPart = HitTest(x, y);
    pimpl()->m_selection_start = pimpl()->m_selection_end = iPart;
    invalidate();
}

void FuriganaCtl::OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags) {
    if (GetCapture() != hwnd)
        return;
    INT iPart = HitTest(x, y);
    pimpl()->m_selection_end = iPart;

    invalidate();
}

void FuriganaCtl::OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags) {
    if (GetCapture() != hwnd)
        return;

    INT iPart = HitTest(x, y);
    pimpl()->m_selection_end = iPart;

    ReleaseCapture();
    invalidate();
}

LRESULT CALLBACK FuriganaCtl::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, OnLButtonUp);
    case FC_SETRUBYRATIO:
        if ((INT)wParam >= 0 && (INT)lParam > 0)
        {
            pimpl()->m_ruby_ratio_mul = (INT)wParam; // ルビ比率の分子
            pimpl()->m_ruby_ratio_div = (INT)lParam; // ルビ比率の分母
            return TRUE;
        }
        return FALSE;
    default:
        return BaseTextBox::window_proc_inner(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void FuriganaCtl::draw_client(HWND hwnd, HDC dc, RECT *client_rc) {
    FillRect(dc, client_rc, GetSysColorBrush(COLOR_WINDOW));

    DWORD style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    DWORD flags = 0;
    if (!(style & ES_MULTILINE)) flags |= DT_SINGLELINE;
    if (style & ES_CENTER) flags |= DT_CENTER;
    if (style & ES_RIGHT) flags |= DT_RIGHT;

    SetPartsSelection(pimpl()->m_parts, pimpl()->m_selection_start, pimpl()->m_selection_end);

    DrawFuriganaOneLineText(
        dc,
        pimpl()->m_text,
        pimpl()->m_parts,
        client_rc,
        pimpl()->m_font,
        pimpl()->m_sub_font,
        pimpl()->m_delta_x,
        flags);
}

void FuriganaCtl::invalidate() {
    ParseRubyCompoundText(pimpl()->m_parts, pimpl()->m_text);
    BaseTextBox::invalidate();
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
