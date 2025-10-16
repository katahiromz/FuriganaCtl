// License: MIT
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "../FuriganaCtl/furigana_api.h"

HFONT g_hFont = NULL;

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    bool multi = true;
    bool center = false;
    bool right = false;
    DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (multi) style |= ES_MULTILINE;
    if (center) style |= ES_CENTER;
    if (right) style |= ES_RIGHT;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -16;
    lf.lfCharSet = SHIFTJIS_CHARSET;
    lstrcpyn(lf.lfFaceName, TEXT("ピザPゴシック"), _countof(lf.lfFaceName));
    g_hFont = ::CreateFontIndirect(&lf);

    SendDlgItemMessage(hwnd, edt2, FC_SETRUBYRATIO, 4, 5);
    SendDlgItemMessage(hwnd, edt2, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    SetDlgItemText(hwnd, edt1, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。第三次世界大戦(だいさんじせかいたいせん)にならないように。志布志(しぶし)"));
    SetDlgItemText(hwnd, edt2, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。第三次世界大戦(だいさんじせかいたいせん)にならないように。志布志(しぶし)"));
    return TRUE;
}

void OnDestroy(HWND hwnd)
{
    DeleteObject(g_hFont);
    g_hFont = NULL;
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case edt1:
        {
            TCHAR text[512];
            GetDlgItemText(hwnd, edt1, text, _countof(text));
            SetDlgItemText(hwnd, edt2, text);
        }
        break;
    }
}

INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    InitCommonControls();
    LoadLibrary(TEXT("FuriganaCtl"));
    DialogBox(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc);
    return 0;
}
