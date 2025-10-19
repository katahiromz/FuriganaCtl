// License: MIT
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "../FuriganaCtl/furigana_api.h"

HFONT g_hFont = NULL;

void SetAlign(HWND hwnd, DWORD align)
{
    HWND hwndEdt2 = GetDlgItem(hwnd, edt2);

    bool center = false;
    bool right = true;
    DWORD style = GetWindowLongPtrW(hwndEdt2, GWL_STYLE);
    style &= ~(ES_CENTER | ES_RIGHT);
    if (align & ES_CENTER) style |= ES_CENTER;
    if (align & ES_RIGHT) style |= ES_RIGHT;
    SetWindowLongPtrW(hwndEdt2, GWL_STYLE, style);

    InvalidateRect(hwndEdt2, NULL, TRUE);
}

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND hwndEdt2 = GetDlgItem(hwnd, edt2);

    RECT margin = { 4, 4, 4, 4 };
    SendMessage(hwndEdt2, FC_SETMARGIN, 0, (LPARAM)&margin);

    SendMessage(hwndEdt2, FC_SETCOLOR, 0, RGB(0, 0, 0));
    SendMessage(hwndEdt2, FC_SETCOLOR, 1, RGB(255, 255, 0));

    bool multi = true;
    DWORD style = GetWindowLongPtrW(hwndEdt2, GWL_STYLE);
    if (multi) style |= ES_MULTILINE;
    SetWindowLongPtrW(hwndEdt2, GWL_STYLE, style);

    CheckRadioButton(hwnd, rad1, rad3, rad1);

    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -16;
    lf.lfCharSet = SHIFTJIS_CHARSET;
    lstrcpyn(lf.lfFaceName, TEXT("ピザPゴシック"), _countof(lf.lfFaceName));
    g_hFont = ::CreateFontIndirect(&lf);

    SendDlgItemMessage(hwnd, edt2, FC_SETRUBYRATIO, 4, 5);
    SendDlgItemMessage(hwnd, edt2, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    SetDlgItemText(hwnd, edt1, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。"));
    SetDlgItemText(hwnd, edt2, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。"));
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
    case rad1:
        SetAlign(hwnd, ES_LEFT);
        break;
    case rad2:
        SetAlign(hwnd, ES_CENTER);
        break;
    case rad3:
        SetAlign(hwnd, ES_RIGHT);
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
