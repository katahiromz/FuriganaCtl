// furigana_ctl.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "furigana_ctl.h"
#include "furigana_api.h"
#include "../furigana_gdi/furigana_gdi.h"
#include <windowsx.h>
#include <new>
#include <cassert>

static inline void out_of_memory() {
    OutputDebugStringA("Out of memory!\n");
    MessageBoxA(nullptr, "Out of memory!", "Error", MB_ICONERROR);
}

//////////////////////////////////////////////////////////////////////////////
// furigana_ctl_impl

#include "furigana_ctl_impl.h"

void furigana_ctl_impl::OnSetFont(HWND hwndCtl, HFONT hfont, BOOL fRedraw) {
    if (!hfont)
        return;

    LOGFONT lf;
    ::GetObject(hfont, sizeof(lf), &lf);
    lf.lfHeight *= m_ruby_ratio_mul;
    lf.lfHeight /= m_ruby_ratio_div;

    if (m_own_sub_font && m_sub_font)
        ::DeleteObject(m_sub_font);

    m_sub_font = ::CreateFontIndirect(&lf);

    base_textbox_impl::OnSetFont(hwndCtl, hfont, fRedraw);

    ::InvalidateRect(hwndCtl, nullptr, FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// furigana_ctl

IMPLEMENT_DYNAMIC(furigana_ctl);

furigana_ctl::furigana_ctl(HWND hwnd) {
    delete m_pimpl;
    m_pimpl = new furigana_ctl_impl(hwnd, this);
    if (!m_pimpl) {
        out_of_memory();
    }
}

furigana_ctl::~furigana_ctl() { }

BOOL furigana_ctl::register_class(HINSTANCE inst) {
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

BOOL furigana_ctl::unregister_class(HINSTANCE inst) {
    return ::UnregisterClassW(get_class_name(), inst);
}

LRESULT CALLBACK furigana_ctl::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg)
    {
    case FC_SETRUBYRATIO:
        if ((INT)wParam >= 0 && (INT)lParam > 0)
        {
            pimpl()->m_ruby_ratio_mul = (INT)wParam; // ルビ比率の分子
            pimpl()->m_ruby_ratio_div = (INT)lParam; // ルビ比率の分母
            return TRUE;
        }
        return FALSE;
    default:
        return base_textbox::window_proc_inner(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void furigana_ctl::draw_client(HWND hwnd, HDC dc, RECT *client_rc) {
    FillRect(dc, client_rc, GetStockBrush(WHITE_BRUSH));

    DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
    DWORD flags = 0;
    if (!(style & ES_MULTILINE))
        flags |= DT_SINGLELINE;

    DrawFuriganaTextLine(dc, get_text(), client_rc, pimpl()->m_font, pimpl()->m_sub_font, flags);
}

//////////////////////////////////////////////////////////////////////////////
// DllMain

#ifdef FURIGANA_CTL_EXPORT
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        furigana_ctl::register_class(nullptr);
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        furigana_ctl::unregister_class(nullptr);
        break;
    }
    return TRUE;
}
#endif  // def FURIGANA_CTL_EXPORT
