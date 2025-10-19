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
#include "resource.h"

// メモリ不足時のメッセージ
static inline void out_of_memory() {
    OutputDebugStringA("Out of memory!\n");
    MessageBoxA(NULL, "Out of memory!", "Error", MB_ICONERROR);
}

// デバッグ出力
static inline void DPRINTF(LPCWSTR fmt, ...) {
#ifndef NDEBUG
    WCHAR text[1024];
    va_list va;
    va_start(va, fmt);
    INT len = wvsprintfW(text, fmt, va);
    assert(len < _countof(text));
    OutputDebugStringW(text);
    va_end(va);
#endif
}

// インスタンス。メニューを読み込むのに使う。
static HINSTANCE s_hinst = NULL;

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl_impl - FuriganaCtl実装用構造体

#include "FuriganaCtl_impl.h"

void FuriganaCtl_impl::reset_color(INT iColor) {
    switch (iColor) {
    case 0: m_colors[iColor] = ::GetSysColor(COLOR_WINDOWTEXT); break;
    case 1: m_colors[iColor] = ::GetSysColor(COLOR_WINDOW); break;
    case 2: m_colors[iColor] = ::GetSysColor(COLOR_HIGHLIGHTTEXT); break;
    case 3: m_colors[iColor] = ::GetSysColor(COLOR_HIGHLIGHT); break;
    default:
        return;
    }
    m_color_is_set[iColor] = false;
}

// 色をリセットする
void FuriganaCtl_impl::reset_colors() {
    for (size_t iColor = 0; iColor < _countof(m_colors); ++iColor) {
        reset_color((INT)iColor);
    }
}

void FuriganaCtl_impl::update_scroll_info() {
    RECT rc;
    ::GetClientRect(m_hwnd, &rc);
    rc.left += m_margin_rect.left;
    rc.top += m_margin_rect.top;
    rc.right -= m_margin_rect.right;
    rc.bottom -= m_margin_rect.bottom;

    RECT rcIdeal = rc;
    m_doc.get_ideal_size(&rcIdeal, get_draw_flags());

    // 横スクロール設定: nMax は「コンテンツ幅 - ページ幅」
    INT pageW = max(0, rc.right - rc.left);
    INT docW  = max(0, rcIdeal.right - rcIdeal.left);
    INT maxHorz = max(0, docW - pageW);

    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_PAGE | SIF_RANGE;

    si.nPage = pageW;
    si.nMax = maxHorz;
    ::SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);

    // 縦スクロール設定: nMax は「コンテンツ高さ - ページ高さ」
    INT pageH = max(0, rc.bottom - rc.top);
    INT docH  = max(0, rcIdeal.bottom - rcIdeal.top);
    INT maxVert = max(0, docH - pageH);

    si.nPage = pageH;
    si.nMax = maxVert;
    ::SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

// 無効にして再描画
void FuriganaCtl_impl::invalidate() {
    if (m_doc.m_text != m_text) {
        m_doc.clear();
        m_doc.add_text(m_text);
    }

    update_scroll_info();

    BaseTextBox_impl::invalidate();
}

// 描画フラグ群を取得
UINT FuriganaCtl_impl::get_draw_flags() const {
    DWORD style = (DWORD)GetWindowLongPtrW(m_hwnd, GWL_STYLE);

    UINT flags = 0;
    if (!(style & ES_MULTILINE)) flags |= DT_SINGLELINE;
    if (style & ES_CENTER) flags |= DT_CENTER;
    if (style & ES_RIGHT) flags |= DT_RIGHT;
    return flags;
}

// FC_SETRUBYRATIO
LRESULT FuriganaCtl_impl::OnSetRubyRatio(INT mul, INT div) {
    if (mul >= 0 && div > 0) {
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
        reset_color(iColor);
        return TRUE;
    }

    m_colors[iColor] = rgbColor;
    m_color_is_set[iColor] = true;
    invalidate();
    return TRUE;
}

// FC_SETLINEGAP
LRESULT FuriganaCtl_impl::OnSetLineGap(INT line_gap) {
    if (line_gap < 0)
        return FALSE;
    m_doc.m_line_gap = line_gap;
    invalidate();
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
void FuriganaCtl_impl::OnKey(HWND hwnd, UINT vk, BOOL fDown, INT cRepeat, UINT flags)
{
    if (!fDown)
        return;

    BOOL fShift = GetKeyState(VK_SHIFT) < 0;
    BOOL fCtrl = GetKeyState(VK_CONTROL) < 0;
    INT iStart = m_doc.m_selection_start, iEnd = m_doc.m_selection_end;
    INT cParts = (INT)m_doc.m_parts.size();
    switch (vk) {
    case L'C':
        if (fCtrl) // Ctrl+C
            OnCopy(hwnd);
        break;
    case 'A':
        if (fCtrl) // Ctrl+A
            select_all();
        break;
    case VK_PRIOR: // PageUp
        ::PostMessageW(hwnd, WM_VSCROLL, MAKELPARAM(SB_PAGEUP, 0), 0);
        break;
    case VK_NEXT: // PageDown
        ::PostMessageW(hwnd, WM_VSCROLL, MAKELPARAM(SB_PAGEDOWN, 0), 0);
        break;
    case VK_HOME: // Home キー
        fCtrl = TRUE;
        // FALL THROUGH
    case VK_LEFT: // ←
        if (iStart == -1 || iEnd == -1) {
            m_doc.set_selection(0, 0);
            break;
        }
        if (fShift) { // Shiftが押されている？
            if (fCtrl) { // Ctrlが押されている？
                iEnd = 0;
            } else {
                if (iEnd > 0)
                    --iEnd;
            }
        } else {
            if (fCtrl) { // Ctrlが押されている？
                iStart = iEnd = 0;
            } else if (iStart == iEnd) {
                if (iEnd > 0) {
                    --iStart;
                    --iEnd;
                }
            } else if (iStart < iEnd) {
                iEnd = iStart;
            } else {
                iStart = iEnd;
            }
        }
        m_doc.set_selection(iStart, iEnd);
        invalidate();
        break;
    case VK_END: // End キー
        fCtrl = TRUE;
        // FALL THROUGH
    case VK_RIGHT: // →
        if (iStart == -1 || iEnd == -1) {
            m_doc.set_selection(cParts, cParts);
            break;
        }
        if (fShift) { // Shiftが押されている？
            if (fCtrl) { // Ctrlが押されている？
                iEnd = cParts;
            } else {
                if (iEnd < cParts)
                    ++iEnd;
            }
        } else {
            if (fCtrl) { // Ctrlが押されている？
                iStart = iEnd = cParts;
            } else if (iStart == iEnd) {
                if (iEnd < cParts) {
                    ++iStart;
                    ++iEnd;
                }
            } else if (iStart < iEnd) {
                iStart = iEnd;
            } else {
                iEnd = iStart;
            }
        }
        m_doc.set_selection(iStart, iEnd);
        invalidate();
        break;
    default:
        break;
    }

}

// WM_GETDLGCODE
UINT FuriganaCtl_impl::OnGetDlgCode(HWND hwnd, LPMSG lpmsg) {
    return DLGC_WANTALLKEYS;
}

// WM_RBUTTONDOWN
void FuriganaCtl_impl::OnRButtonDown(HWND hwnd, BOOL fDoubleClick, INT x, INT y, UINT keyFlags) {
    SetFocus(hwnd);
}

LRESULT FuriganaCtl_impl::OnSetSel(INT iStartSel, INT iEndSel) {
    m_doc.m_selection_start = iStartSel;
    m_doc.m_selection_end = iEndSel;
    m_doc.update_selection();
    invalidate();
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

    HMENU hMenu = ::LoadMenu(s_hinst ? s_hinst : GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    HMENU hSubMenu = ::GetSubMenu(hMenu, 0);
    if (hSubMenu) {
        if (m_doc.m_selection_start == -1 || m_doc.m_selection_start == m_doc.m_selection_end)
            ::EnableMenuItem(hSubMenu, ID_COPY, MF_GRAYED); // 選択領域がなければコピーを無効化

        if (m_doc.m_text.empty()) // テキストがなければ
            ::EnableMenuItem(hSubMenu, ID_SELECTALL, MF_GRAYED); // 「すべて選択」を無効に

        // TrackPopupMenuの不具合の回避策
        SetForegroundWindow(hwnd);

        UINT uFlags = TPM_RIGHTBUTTON | TPM_RETURNCMD;
        INT id = TrackPopupMenu(hSubMenu, uFlags, xPos, yPos, 0, hwnd, NULL);
        if (id != 0 && id != -1) {
            switch (id) {
            case ID_COPY: // コピー
                OnCopy(hwnd);
                break;
            case ID_SELECTALL: // すべて選択
                select_all();
                break;
            default:
                assert(0);
                break;
            }
        }

        // TrackPopupMenuの不具合の回避策
        PostMessageW(hwnd, WM_NULL, 0, 0);
    }
    ::DestroyMenu(hMenu);
}

// WM_MOUSEWHEEL
void FuriganaCtl_impl::OnMouseWheel(HWND hwnd, INT xPos, INT yPos, INT zDelta, UINT fwKeys)
{
    if (::GetKeyState(VK_SHIFT) < 0) {
        if (zDelta < 0)
            ::PostMessageW(hwnd, WM_HSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
        else if (zDelta > 0)
            ::PostMessageW(hwnd, WM_HSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
    } else {
        if (zDelta < 0)
            ::PostMessageW(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
        else if (zDelta > 0)
            ::PostMessageW(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
    }
}

// WM_COPY
void FuriganaCtl_impl::OnCopy(HWND hwnd) {
    std::wstring text = m_doc.get_selection_text();

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

// 当たり判定
INT FuriganaCtl_impl::hit_test(INT x, INT y) {
    // Use the stored scroll offsets (m_scroll_x/m_scroll_y) rather than querying
    // the scrollbar control via GetScrollPos(). This avoids races / inconsistencies
    // between the control's scroll position and the internal m_scroll_* values
    // used for painting.
    x += m_scroll_x - m_margin_rect.left;
    y += m_scroll_y - m_margin_rect.top;
    return m_doc.hit_test(x, y);
}

// WM_LBUTTONDOWN
void FuriganaCtl_impl::OnLButtonDown(HWND hwnd, BOOL fDoubleClick, INT x, INT y, UINT keyFlags) {
    DPRINTF(L"OnLButtonDown: %d, %d, %p\n", x, y, GetCapture());
    SetFocus(hwnd);
    SetCapture(hwnd);

    INT iPart = hit_test(x, y);
    m_doc.set_selection(iPart, iPart);
    invalidate();
}

// WM_MOUSEMOVE
void FuriganaCtl_impl::OnMouseMove(HWND hwnd, INT x, INT y, UINT keyFlags) {
    //DPRINTF(L"OnMouseMove: %d, %d, %p\n", x, y, GetCapture());
    if (::GetCapture() != hwnd) {
        return;
    }

    INT iPart = hit_test(x, y);
    m_doc.set_selection(m_doc.m_selection_start, iPart);

    invalidate();
}

// WM_LBUTTONUP
void FuriganaCtl_impl::OnLButtonUp(HWND hwnd, INT x, INT y, UINT keyFlags) {
    DPRINTF(L"OnLButtonUp: %d, %d, %p\n", x, y, GetCapture());
    if (::GetCapture() != hwnd) {
        return;
    }

    INT iPart = hit_test(x, y);
    m_doc.set_selection(m_doc.m_selection_start, iPart);

    ::ReleaseCapture();
    invalidate();
}

// WM_SYSCOLORCHANGE
void FuriganaCtl_impl::OnSysColorChange(HWND hwnd) {
    INT iColor = 0;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID); ++iColor;
    if (!m_color_is_set[iColor]) OnSetColor(iColor, CLR_INVALID);
    invalidate();
}

// WM_SIZE
void FuriganaCtl_impl::OnSize(HWND hwnd, UINT state, INT cx, INT cy) {
    // クライアントサイズからスクロール範囲を決定する
    RECT rcClient = {0, 0, cx, cy};
    RECT rc = rcClient;
    rc.left += m_margin_rect.left;
    rc.top += m_margin_rect.top;
    rc.right -= m_margin_rect.right;
    rc.bottom -= m_margin_rect.bottom;

    // flags: シングルラインかどうか
    DWORD flags = get_draw_flags();

    // doc のサイズ計算（描画計測）
    RECT rcIdeal = rc;
    m_doc.get_ideal_size(&rcIdeal, flags);
    INT doc_width = m_doc.m_para_width;
    INT doc_height = m_doc.m_para_height;

    // 垂直スクロール設定
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = max(0, doc_height - (rc.bottom - rc.top));
    si.nPage = max(0, rc.bottom - rc.top);
    si.nPos = m_scroll_y;
    if (si.nPos > si.nMax) si.nPos = si.nMax;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    m_scroll_y = si.nPos;

    // 水平スクロール設定（単一行のとき等）
    si.nMin = 0;
    si.nMax = max(0, doc_width - (rc.right - rc.left));
    si.nPage = max(0, rc.right - rc.left);
    si.nPos = m_scroll_x;
    if (si.nPos > si.nMax) si.nPos = si.nMax;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    m_scroll_x = si.nPos;
}

// WM_HSCROLL
void FuriganaCtl_impl::OnHScroll(HWND hwnd, HWND hwndCtl, UINT code, INT pos) {
    update_scroll_info();

    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_ALL;
    ::GetScrollInfo(hwnd, SB_HORZ, &si);

    INT nPos = si.nPos;
    switch (code) {
    case SB_LINELEFT:
        nPos = max(si.nMin, nPos - 1);
        break;
    case SB_LINERIGHT:
        nPos = min(si.nMax, nPos + 1);
        break;
    case SB_PAGELEFT:
        nPos = max(si.nMin, nPos - (INT)si.nPage);
        break;
    case SB_PAGERIGHT:
        nPos = min(si.nMax, nPos + (INT)si.nPage);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        nPos = pos;
        break;
    case SB_LEFT:
        nPos = si.nMin;
        break;
    case SB_RIGHT:
        nPos = si.nMax;
        break;
    default:
        return;
    }

    if (nPos != si.nPos) {
        si.fMask = SIF_POS;
        si.nPos = nPos;
        ::SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        m_scroll_x = nPos;
        invalidate();
    }
}

// WM_VSCROLL
void FuriganaCtl_impl::OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, INT pos) {
    update_scroll_info();

    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_ALL;
    ::GetScrollInfo(hwnd, SB_VERT, &si);

    INT nPos = si.nPos;
    switch (code) {
    case SB_LINEUP:
        nPos = max(si.nMin, nPos - 1);
        break;
    case SB_LINEDOWN:
        nPos = min(si.nMax, nPos + 1);
        break;
    case SB_PAGEUP:
        nPos = max(si.nMin, nPos - (INT)si.nPage);
        break;
    case SB_PAGEDOWN:
        nPos = min(si.nMax, nPos + (INT)si.nPage);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        nPos = pos;
        break;
    case SB_TOP:
        nPos = si.nMin;
        break;
    case SB_BOTTOM:
        nPos = si.nMax;
        break;
    default:
        return;
    }

    if (nPos != si.nPos) {
        si.fMask = SIF_POS;
        si.nPos = nPos;
        ::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        m_scroll_y = nPos;
        invalidate();
    }
}

// WM_PAINT
void FuriganaCtl_impl::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = ::BeginPaint(hwnd, &ps);
    if (!dc) return;

    // クライアント領域
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    INT cx = rcClient.right - rcClient.left;
    INT cy = rcClient.bottom - rcClient.top;
    assert(cx > 0 && cy > 0);

    // メモリDCの作成（ダブルバッファ）
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP hbm = CreateCompatibleBitmap(dc, cx, cy);
    HGDIOBJ oldBmp = SelectObject(memDC, hbm);

    paint_client_inner(hwnd, memDC, &rcClient);

    // ビットブロットで画面へ転送
    BitBlt(dc, 0, 0, cx, cy, memDC, 0, 0, SRCCOPY);

    // 後片付け
    SelectObject(memDC, oldBmp);
    DeleteObject(hbm);
    DeleteDC(memDC);

    ::EndPaint(hwnd, &ps);
}

// クライアント領域を描画する
void FuriganaCtl_impl::paint_client_inner(HWND hwnd, HDC dc, RECT *client_rect) {
    RECT rc = *client_rect;

    // 背景を塗りつぶす
    HBRUSH hBrush = ::CreateSolidBrush(m_colors[1]);
    ::FillRect(dc, &rc, hBrush);
    ::DeleteObject(hBrush);

    // 余白を空ける
    rc.left += m_margin_rect.left;
    rc.top += m_margin_rect.top;
    rc.right -= m_margin_rect.right;
    rc.bottom -= m_margin_rect.bottom;

    // スクロールを反映するためにビューポート原点を移す（負値）
    ::SetViewportOrgEx(dc, -m_scroll_x, -m_scroll_y, NULL);

    // 選択領域を更新
    m_doc.update_selection();

    // 描画
    m_doc.draw_doc(dc, &rc, get_draw_flags(), m_colors);
}

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl

// 動的な生成を可能にする。
IMPLEMENT_DYNAMIC(FuriganaCtl);

FuriganaCtl::FuriganaCtl(HWND hwnd) {
    delete m_pimpl;
    m_pimpl = new FuriganaCtl_impl(hwnd, this);
    if (!m_pimpl) {
        out_of_memory();
    }
}

FuriganaCtl::~FuriganaCtl() { }

// ウィンドウ クラスの登録
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

// 登録の解除
BOOL FuriganaCtl::unregister_class(HINSTANCE inst) {
    return ::UnregisterClassW(get_class_name(), inst);
}

// 内部ウィンドウ プロシージャ
LRESULT CALLBACK FuriganaCtl::window_proc_inner(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_PAINT, pimpl()->OnPaint);
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, pimpl()->OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, pimpl()->OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, pimpl()->OnLButtonUp);
        HANDLE_MSG(hwnd, WM_RBUTTONDOWN, pimpl()->OnRButtonDown);
        HANDLE_MSG(hwnd, WM_SYSCOLORCHANGE, pimpl()->OnSysColorChange);
        HANDLE_MSG(hwnd, WM_COPY, pimpl()->OnCopy);
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, pimpl()->OnMouseWheel);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, pimpl()->OnContextMenu);
        HANDLE_MSG(hwnd, WM_GETDLGCODE, pimpl()->OnGetDlgCode);
        HANDLE_MSG(hwnd, WM_KEYDOWN, pimpl()->OnKey);
        HANDLE_MSG(hwnd, WM_HSCROLL, pimpl()->OnHScroll);
        HANDLE_MSG(hwnd, WM_VSCROLL, pimpl()->OnVScroll);
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
// DllMain - DLLのメイン関数

#ifdef FURIGANA_CTL_EXPORT
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        s_hinst = hinstDLL;
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
