// License: MIT
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    SetDlgItemText(hwnd, edt1, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。第三次世界大戦(だいさんじせかいたいせん)にならないように。志布志(しぶし)"));
    SetDlgItemText(hwnd, edt2, TEXT("志(こころざし)を持(も)って漢字(かんじ)の振(ふ)り仮名(がな)に携(たずさ)わる。第三次世界大戦(だいさんじせかいたいせん)にならないように。志布志(しぶし)"));
    return TRUE;
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
    LoadLibrary(TEXT("furigana_ctl"));
    DialogBox(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc);
    return 0;
}
