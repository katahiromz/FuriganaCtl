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

// FIXME: 醜いコード
#undef min
#undef max
#include <algorithm>
#define min std::min
#define max std::max

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
    DPRINTF(L"ideal_size: %ld, %ld\n", rcIdeal.right - rcIdeal.left, rcIdeal.bottom - rcIdeal.top);

    INT pageW = max(0, INT(rc.right - rc.left));
    INT docW  = max(0, INT(rcIdeal.right - rcIdeal.left));
    INT maxHorz = max(0, docW);

    SCROLLINFO si = { sizeof(si) };

    si.fMask = SIF_PAGE | SIF_RANGE;
    si.nPage = pageW;
    si.nMax = maxHorz;
    ::SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
    m_scroll_x = ::GetScrollPos(m_hwnd, SB_HORZ);

    INT pageH = max(0, INT(rc.bottom - rc.top));
    INT docH  = max(0, INT(rcIdeal.bottom - rcIdeal.top));
    INT maxVert = max(0, docH);

    si.fMask = SIF_PAGE | SIF_RANGE;
    si.nPage = pageH;
    si.nMax = maxVert;
    ::SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
    m_scroll_y = ::GetScrollPos(m_hwnd, SB_VERT);

    BaseTextBox_impl::invalidate();
}

// 無効にして再描画
void FuriganaCtl_impl::invalidate() {
    if (m_doc.m_text != m_text) {
        m_doc.clear();
        m_doc.add_text(m_text, get_draw_flags());
    }

    update_scroll_info();
}

// 描画フラグ群を取得
UINT FuriganaCtl_impl::get_draw_flags() const {
    DWORD style = m_self->get_style();

    UINT flags = 0;
    if (!(style & ES_MULTILINE) || ((style & ES_AUTOHSCROLL) && !(style & (ES_RIGHT | ES_CENTER))))
        flags |= DT_SINGLELINE;
    if (style & ES_CENTER) flags |= DT_CENTER;
    if (style & ES_RIGHT) flags |= DT_RIGHT;
    return flags;
}

// FC_SETRUBYRATIO
LRESULT FuriganaCtl_impl::OnSetRubyRatio(INT mul, INT div) {
    if (mul >= 0 && div > 0) {
        m_doc.m_ruby_ratio_mul = mul; // ルビ比率の分子
        m_doc.m_ruby_ratio_div = div; // ルビ比率の分母

        OnSetFont(m_hwnd, m_font, TRUE);
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
    if (iColor < 0 || iColor >= _countof(m_colors))
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

// FC_GETIDEALSIZE
LRESULT FuriganaCtl_impl::OnGetIdealSize(INT type, RECT *prc) {
    if (!prc) {
        DPRINTF(L"!prc\n");
        return FALSE;
    }

    ::GetClientRect(m_hwnd, prc);
    m_doc.get_ideal_size(prc, get_draw_flags());

    switch (type) {
    case 0:
        {
            RECT rc;
            SetRect(&rc, 0, 0, prc->right - prc->left, prc->right - prc->left);

            DWORD style = m_self->get_style(), exstyle = m_self->get_exstyle();
            AdjustWindowRectEx(&rc, style, FALSE, exstyle);

            prc->right = prc->left + (rc.right - rc.left) + (m_margin_rect.left + m_margin_rect.right);
            prc->bottom = prc->top + (rc.bottom - rc.top) + (m_margin_rect.top + m_margin_rect.bottom);
        }
        return TRUE;
    case 1:
        return TRUE;
    default:
        DPRINTF(L"unknown type: %d\n", type);
        return FALSE;
    }
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
    if (!m_doc.m_ruby_ratio_div)
        return;

    LOGFONT lf;
    ::GetObject(hfont, sizeof(lf), &lf);
    lf.lfHeight *= m_doc.m_ruby_ratio_mul;
    lf.lfHeight /= m_doc.m_ruby_ratio_div;

    if (m_own_sub_font && m_sub_font)
        ::DeleteObject(m_sub_font);

    m_sub_font = ::CreateFontIndirect(&lf);
    if (m_sub_font) {
        m_doc.set_fonts(m_font, m_sub_font); // 弱い参照
        m_own_sub_font = true;
    } else {
        OutputDebugStringA("CreateFontIndirect failed!\n");
        m_own_sub_font = false;
        // フォールバック：ベースフォントを使用
        m_doc.set_fonts(m_font, m_font); // 弱い参照
    }

    BaseTextBox_impl::OnSetFont(hwndCtl, hfont, fRedraw);
}

// WM_STYLECHANGED
void FuriganaCtl_impl::OnStyleChanged(HWND hwnd) {
    m_doc.set_dirty();
    invalidate();
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
    case VK_UP: // ↑
        {
            // Move caret/selection to the nearest part on the previous visual line.
            // Use iEnd as the active caret position (consistent with Left/Right handling).
            if (iStart == -1 || iEnd == -1) {
                // nothing selected -> place at start
                m_doc.set_selection(0, 0);
                ensure_visible(0);
                invalidate();
                break;
            }

            INT caret = iEnd;
            if (caret < 0) caret = 0;
            if (caret > cParts) caret = cParts;

            // Determine layout parameters
            UINT drawFlags = get_draw_flags();

            // Compute layout width (client area minus margins)
            RECT rcClient;
            ::GetClientRect(m_hwnd, &rcClient);
            RECT rc = rcClient;
            rc.left += m_margin_rect.left;
            rc.top += m_margin_rect.top;
            rc.right -= m_margin_rect.right;
            rc.bottom -= m_margin_rect.bottom;
            INT layout_width = (drawFlags & DT_SINGLELINE) ? MAXLONG : max(0, INT(rc.right - rc.left));

            // Get caret position
            POINT caretPt = {0, 0};
            if (!m_doc.get_part_position(caret, layout_width, &caretPt, drawFlags)) {
                // fallback: do nothing
                break;
            }

            // Find which run contains the caret
            INT runIndex = (INT)m_doc.m_runs.size();
            for (size_t ri = 0; ri < m_doc.m_runs.size(); ++ri) {
                const TextRun& run = m_doc.m_runs[ri];
                if (run.m_part_index_start <= caret && caret < run.m_part_index_end) {
                    runIndex = (INT)ri;
                    break;
                }
            }

            INT newIndex = 0;
            if (runIndex <= 0) {
                // Already on first run: move to start of doc
                newIndex = 0;
            } else {
                // target is previous run
                const TextRun& prevRun = m_doc.m_runs[runIndex - 1];

                // desired x is caretPt.x
                INT desiredX = caretPt.x;

                // iterate parts in prevRun to find the one whose center is closest to desiredX
                INT current_x = prevRun.m_delta_x;
                INT bestPart = prevRun.m_part_index_start;
                INT bestDist = INT_MAX;
                for (INT pi = prevRun.m_part_index_start; pi < prevRun.m_part_index_end; ++pi) {
                    const TextPart &p = m_doc.m_parts[pi];
                    INT center = current_x + p.m_part_width / 2;
                    INT dist = abs(center - desiredX);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPart = pi;
                    }
                    current_x += p.m_part_width;
                }
                newIndex = bestPart;
            }

            // Clamp
            if (newIndex < 0) newIndex = 0;
            if (newIndex > cParts) newIndex = cParts;

            // Update selection based on Shift/Ctrl (Ctrl has no special effect for arrow up here)
            if (fShift) {
                // extend selection to newIndex, keeping anchor at iStart
                // follow convention: keep iStart as anchor, iEnd moves
                if (iStart == -1) iStart = caret; // fallback
                iEnd = newIndex;
            } else {
                iStart = iEnd = newIndex;
            }

            m_doc.set_selection(iStart, iEnd);
            ensure_visible(newIndex);
            invalidate();
        }
        break;
    case VK_DOWN: // ↓
        {
            // Move caret/selection to the nearest part on the next visual line.
            if (iStart == -1 || iEnd == -1) {
                // place at end
                m_doc.set_selection(cParts, cParts);
                ensure_visible(cParts);
                invalidate();
                break;
            }

            INT caret = iEnd;
            if (caret < 0) caret = 0;
            if (caret > cParts) caret = cParts;

            UINT drawFlags = get_draw_flags();

            RECT rcClient;
            ::GetClientRect(m_hwnd, &rcClient);
            RECT rc = rcClient;
            rc.left += m_margin_rect.left;
            rc.top += m_margin_rect.top;
            rc.right -= m_margin_rect.right;
            rc.bottom -= m_margin_rect.bottom;
            INT layout_width = (drawFlags & DT_SINGLELINE) ? MAXLONG : max(0, INT(rc.right - rc.left));

            POINT caretPt = {0, 0};
            if (!m_doc.get_part_position(caret, layout_width, &caretPt, drawFlags)) {
                break;
            }

            // find run containing caret
            INT runIndex = (INT)m_doc.m_runs.size();
            for (size_t ri = 0; ri < m_doc.m_runs.size(); ++ri) {
                const TextRun& run = m_doc.m_runs[ri];
                if (run.m_part_index_start <= caret && caret < run.m_part_index_end) {
                    runIndex = (INT)ri;
                    break;
                }
            }

            INT newIndex = cParts;
            if (runIndex == -1) {
                // caret not inside runs (maybe at end) -> go to last part
                if (!m_doc.m_runs.empty()) {
                    const TextRun& lastRun = m_doc.m_runs.back();
                    newIndex = lastRun.m_part_index_end - 1;
                    if (newIndex < 0) newIndex = 0;
                } else {
                    newIndex = 0;
                }
            } else if (runIndex >= (INT)m_doc.m_runs.size() - 1) {
                // already last run -> go to end
                newIndex = cParts;
            } else {
                const TextRun& nextRun = m_doc.m_runs[runIndex + 1];
                INT desiredX = caretPt.x;

                INT current_x = nextRun.m_delta_x;
                INT bestPart = nextRun.m_part_index_start;
                INT bestDist = INT_MAX;
                for (INT pi = nextRun.m_part_index_start; pi < nextRun.m_part_index_end; ++pi) {
                    const TextPart &p = m_doc.m_parts[pi];
                    INT center = current_x + p.m_part_width / 2;
                    INT dist = abs(center - desiredX);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPart = pi;
                    }
                    current_x += p.m_part_width;
                }
                newIndex = bestPart;
            }

            if (newIndex < 0) newIndex = 0;
            if (newIndex > cParts) newIndex = cParts;

            if (fShift) {
                if (iStart == -1) iStart = caret;
                iEnd = newIndex;
            } else {
                iStart = iEnd = newIndex;
            }

            m_doc.set_selection(iStart, iEnd);
            ensure_visible(newIndex);
            invalidate();
        }
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
        ensure_visible(iEnd);
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
        ensure_visible(iEnd);
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

/**
 * 当たり判定。
 * クライアント座標をドキュメント座標に変換して当たり判定を実行します。
 *
 * @param x クライアント座標のX
 * @param y クライアント座標のY
 * @return ドキュメント座標での当たり判定結果。
 *         具体的には m_doc.hit_test(doc_x, doc_y) の返却値：
 *         - 該当するパートのインデックス（0 ～ パート数-1）
 *         - パートが見つからない場合はパート総数
 */
INT FuriganaCtl_impl::hit_test(INT x, INT y) {
    INT draw_x = x - m_margin_rect.left;
    INT draw_y = y - m_margin_rect.top;
    INT doc_x = draw_x + m_scroll_x;
    INT doc_y = draw_y + m_scroll_y;
    INT iPart = m_doc.hit_test(doc_x, doc_y, get_draw_flags());
#ifndef NDEBUG // デバッグ時のみ
    std::wstring text;
    if (iPart < (INT)m_doc.m_parts.size()) {
        text = m_doc.m_parts[iPart].m_text;
    }
    DPRINTF(L"hit_test: %d, \"%ls\"\n", iPart, text.c_str());
#endif
    return iPart;
}

// ensure_visible: 指定されたパートがクライアント領域内に入るようにスクロール位置を調整します。
// iPart: パートインデックス（m_doc.m_parts のインデックス）
void FuriganaCtl_impl::ensure_visible(INT iPart) {
    if (iPart < 0)
        iPart = 0;
    if (iPart >= (INT)m_doc.m_parts.size())
        iPart = (INT)m_doc.m_parts.size();

    // クライアントの描画領域（マージンを除く）
    RECT rcClient;
    ::GetClientRect(m_hwnd, &rcClient);
    RECT rc = rcClient;
    rc.left += m_margin_rect.left;
    rc.top += m_margin_rect.top;
    rc.right -= m_margin_rect.right;
    rc.bottom -= m_margin_rect.bottom;

    UINT flags = get_draw_flags(); // 描画フラグ

    // 折り返し幅（単一行モードなら無視される）
    INT layout_width = (flags & DT_SINGLELINE) ? MAXLONG : max(0, INT(rc.right - rc.left));

    POINT pt = {0, 0};
    if (!m_doc.get_part_position(iPart, layout_width, &pt, flags))
        return;

    // run 高さとパート幅を取得（垂直スクロール調整に使用）
    INT part_width = 0, run_height = 0;
    if (iPart < (INT)m_doc.m_parts.size()) {
        part_width = m_doc.m_parts[iPart].m_part_width;
        run_height = 0;
        for (size_t ri = 0; ri < m_doc.m_runs.size(); ++ri) {
            const TextRun& run = m_doc.m_runs[ri];
            if (run.m_part_index_start <= iPart && iPart < run.m_part_index_end) {
                run_height = run.m_run_height;
                break;
            }
        }
    }

    // 現在のページサイズ
    INT pageW = max(0, INT(rc.right - rc.left));
    INT pageH = max(0, INT(rc.bottom - rc.top));

    // 更新前にスクロール情報を最新化して範囲を得る
    update_scroll_info();

    // 取得されたスクロール範囲を得る
    SCROLLINFO siH = { sizeof(siH) };
    siH.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    ::GetScrollInfo(m_hwnd, SB_HORZ, &siH);
    INT maxH = siH.nMax;
    INT minH = siH.nMin;
    INT pageHx = (INT)siH.nPage;

    SCROLLINFO siV = { sizeof(siV) };
    siV.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    ::GetScrollInfo(m_hwnd, SB_VERT, &siV);
    INT maxV = siV.nMax;
    INT minV = siV.nMin;
    INT pageVy = (INT)siV.nPage;

    INT new_scroll_x = m_scroll_x;
    INT new_scroll_y = m_scroll_y;

    // 横スクロール：単一行または必要に応じて
    if (flags & DT_SINGLELINE) {
        if (pt.x < m_scroll_x) {
            new_scroll_x = pt.x;
        } else if (pt.x + part_width > m_scroll_x + pageW) {
            new_scroll_x = pt.x + part_width - pageW;
        }
    } else {
        // 複数行でも横方向に見切れがあるなら調整（稀なケース）
        if (pt.x < m_scroll_x) {
            new_scroll_x = pt.x;
        } else if (pt.x + part_width > m_scroll_x + pageW) {
            new_scroll_x = pt.x + part_width - pageW;
        }
    }

    // 縦スクロール
    if (pt.y < m_scroll_y) {
        new_scroll_y = pt.y;
    } else if (pt.y + run_height > m_scroll_y + pageH) {
        new_scroll_y = pt.y + run_height - pageH;
    }

    // clamp
    if (new_scroll_x < minH) new_scroll_x = minH;
    if (new_scroll_x > maxH) new_scroll_x = maxH;
    if (new_scroll_y < minV) new_scroll_y = minV;
    if (new_scroll_y > maxV) new_scroll_y = maxV;

    // 変更があれば反映
    bool changed = false;
    if (new_scroll_x != m_scroll_x) {
        m_scroll_x = new_scroll_x;
        SCROLLINFO si = { sizeof(si) };
        si.fMask = SIF_POS;
        si.nPos = m_scroll_x;
        ::SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
        changed = true;
    }
    if (new_scroll_y != m_scroll_y) {
        m_scroll_y = new_scroll_y;
        SCROLLINFO si = { sizeof(si) };
        si.fMask = SIF_POS;
        si.nPos = m_scroll_y;
        ::SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        changed = true;
    }

    if (changed)
        invalidate();
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
    ensure_visible(iPart);

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
    ensure_visible(iPart);

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
    // 理想的なサイズを取得
    RECT rc = { 0, 0, cx, cy };
    ::SendMessageW(hwnd, FC_GETIDEALSIZE, 1, (LPARAM)&rc);

    // 内部サイズ
    INT cxInner = cx - (m_margin_rect.left + m_margin_rect.right);
    INT cyInner = cy - (m_margin_rect.top + m_margin_rect.bottom);

    SCROLLINFO si = { sizeof(si) };

    // 水平スクロール設定（単一行のとき等）
    si.nMax = max(0, INT(rc.top));
    si.nPage = max(0, cxInner);
    si.nPos = m_scroll_x;
    if (si.nPos > si.nMax) si.nPos = si.nMax;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    m_scroll_x = si.nPos;

    // 垂直スクロール設定
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMax = max(0, INT(rc.bottom));
    si.nPage = max(0, cyInner);
    si.nPos = m_scroll_y;
    if (si.nPos > si.nMax) si.nPos = si.nMax;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    m_scroll_y = si.nPos;
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
        nPos = max(si.nMin, nPos - m_scroll_step_x);
        break;
    case SB_LINERIGHT:
        {
            INT maxPos = max(0, (INT)si.nMax - (INT)si.nPage);
            nPos = min(maxPos, nPos + m_scroll_step_x);
        }
        break;
    case SB_PAGELEFT:
        nPos = max(si.nMin, nPos - (INT)si.nPage);
        break;
    case SB_PAGERIGHT:
        {
            INT maxPos = max(0, (INT)si.nMax - (INT)si.nPage);
            nPos = min(maxPos, nPos + (INT)si.nPage);
        }
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
        m_scroll_x = nPos;
        si.fMask = SIF_POS;
        si.nPos = nPos;
        ::SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
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
        nPos = max(si.nMin, nPos - m_scroll_step_y);
        break;
    case SB_LINEDOWN:
        {
            INT maxPos = max(0, (INT)si.nMax - (INT)si.nPage);
            nPos = min(maxPos, nPos + m_scroll_step_y);
        }
        break;
    case SB_PAGEUP:
        nPos = max(si.nMin, nPos - (INT)si.nPage);
        break;
    case SB_PAGEDOWN:
        {
            INT maxPos = max(0, (INT)si.nMax - (INT)si.nPage);
            nPos = min(maxPos, nPos + (INT)si.nPage);
        }
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
        m_scroll_y = nPos;
        si.fMask = SIF_POS;
        si.nPos = nPos;
        ::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
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
    if (cx <= 0 || cy <= 0) {
        ::EndPaint(hwnd, &ps);
        return;
    }

    // メモリDCの作成（ダブルバッファ）
    HDC memDC = ::CreateCompatibleDC(dc); // DC作成
    if (!memDC) {
        ::EndPaint(hwnd, &ps);
        return;
    }

    HBITMAP hbm = ::CreateCompatibleBitmap(dc, cx, cy); // ビットマップ作成
    if (!hbm) {
        ::DeleteDC(memDC);
        ::EndPaint(hwnd, &ps);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, hbm); // ビットマップ選択
    if (!oldBmp) {
        ::DeleteObject(hbm);
        ::DeleteDC(memDC);
        ::EndPaint(hwnd, &ps);
        return;
    }

    // 内部描画
    paint_inner(hwnd, memDC, &rcClient);

    // ビットブロットで画面へ転送
    BitBlt(dc, 0, 0, cx, cy, memDC, 0, 0, SRCCOPY);

    // 後片付け
    SelectObject(memDC, oldBmp);
    DeleteObject(hbm);
    DeleteDC(memDC);

    ::EndPaint(hwnd, &ps);
}

// 描画する
void FuriganaCtl_impl::paint_inner(HWND hwnd, HDC dc, RECT *rect) {
    RECT rc = *rect;

    // 背景を塗りつぶす
    HBRUSH hBrush = ::CreateSolidBrush(m_colors[1]);
    ::FillRect(dc, &rc, hBrush);
    ::DeleteObject(hBrush);

    // 余白を空ける
    rc.left += m_margin_rect.left;
    rc.top += m_margin_rect.top;
    rc.right -= m_margin_rect.right;
    rc.bottom -= m_margin_rect.bottom;

    // スクロールを反映する
    ::OffsetRect(&rc, -m_scroll_x, -m_scroll_y);

    // 描画
    m_doc.draw_doc(dc, &rc, get_draw_flags(), m_colors);
}

//////////////////////////////////////////////////////////////////////////////
// FuriganaCtl

// 動的な生成を可能にする。
IMPLEMENT_DYNAMIC(FuriganaCtl);

FuriganaCtl::FuriganaCtl() {
    m_pimpl = new(std::nothrow) FuriganaCtl_impl(this);
    if (!m_pimpl) {
        OutputDebugStringA("Out of memory!\n");
        assert(0);
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
    FuriganaCtl_impl *pImpl = pimpl();
    if (!pImpl)
        return BaseTextBox::window_proc_inner(hwnd, uMsg, wParam, lParam);
    switch (uMsg) {
        HANDLE_MSG(hwnd, WM_PAINT, pImpl->OnPaint);
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, pImpl->OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, pImpl->OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, pImpl->OnLButtonUp);
        HANDLE_MSG(hwnd, WM_RBUTTONDOWN, pImpl->OnRButtonDown);
        HANDLE_MSG(hwnd, WM_SYSCOLORCHANGE, pImpl->OnSysColorChange);
        HANDLE_MSG(hwnd, WM_COPY, pImpl->OnCopy);
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, pImpl->OnMouseWheel);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, pImpl->OnContextMenu);
        HANDLE_MSG(hwnd, WM_GETDLGCODE, pImpl->OnGetDlgCode);
        HANDLE_MSG(hwnd, WM_KEYDOWN, pImpl->OnKey);
        HANDLE_MSG(hwnd, WM_HSCROLL, pImpl->OnHScroll);
        HANDLE_MSG(hwnd, WM_VSCROLL, pImpl->OnVScroll);
    case WM_STYLECHANGED:
        pImpl->OnStyleChanged(hwnd);
        break;
    case FC_SETRUBYRATIO:
        return pImpl->OnSetRubyRatio((INT)wParam, (INT)lParam);
    case FC_SETMARGIN:
        return pImpl->OnSetMargin((RECT *)lParam);
    case FC_SETCOLOR:
        return pImpl->OnSetColor((INT)wParam, (COLORREF)lParam);
    case FC_SETLINEGAP:
        return pImpl->OnSetLineGap((INT)wParam);
    case FC_GETIDEALSIZE:
        return pImpl->OnGetIdealSize((INT)wParam, (RECT *)lParam);
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
