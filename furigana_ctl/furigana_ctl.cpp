// furigana_ctl.cpp
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include "furigana_ctl.h"
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
    default:
        return base_textbox::window_proc_inner(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void furigana_ctl::draw_client(HWND hwnd, HDC dc, RECT *client_rc) {
    FillRect(dc, client_rc, GetStockBrush(WHITE_BRUSH));
    DrawFuriganaTextLine(dc, get_text(), get_text_length(), 0, client_rc, pimpl()->m_font, pimpl()->m_font);
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
