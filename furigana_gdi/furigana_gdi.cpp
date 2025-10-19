// furigana_gdi.cpp
/////////////////////////////////////////////////////////////////////////////

#include "furigana_gdi.h"
#include "char_judge.h"
#include <assert.h>

#undef min
#undef max

/**
 * テキストの幅を計測する。
 * @param dc 描画するときはデバイスコンテキスト。
 * @param text テキスト。
 * @param len テキストの長さ。
 * @return テキストの幅。
 */
static INT get_text_width(HDC dc, LPCWSTR text, INT len) {
    if (len <= 0) return 0;
    SIZE size = {0};
    GetTextExtentPoint32W(dc, text, len, &size);
    return size.cx;
}

static const COLORREF *get_default_colors() {
    static COLORREF s_colors[4];
    s_colors[0] = GetSysColor(COLOR_WINDOWTEXT);
    s_colors[1] = GetSysColor(COLOR_WINDOW);
    s_colors[2] = GetSysColor(COLOR_HIGHLIGHTTEXT);
    s_colors[3] = GetSysColor(COLOR_HIGHLIGHT);
    return s_colors;
}

/////////////////////////////////////////////////////////////////////////////
// TextPart - テキストのパート。

/**
 * パートの幅を計測する。
 * @param doc 文書。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextPart::UpdateWidth(TextDoc& doc, HFONT hBaseFont, HFONT hRubyFont) {
    // 幅の計測
    HDC dc = doc.m_dc;
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);
    std::wstring& text = doc.m_text;
    switch (m_type) {
    case TextPart::NORMAL:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        m_part_width = m_base_width;
        m_ruby_width = 0;
        break;
    case TextPart::RUBY:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        SelectObject(dc, hRubyFont);
        m_ruby_width = get_text_width(dc, &text[m_ruby_index], m_ruby_len);
        // ルビブロックの幅は、ベースとルビの幅の大きい方
        m_part_width = std::max(m_base_width, m_ruby_width);
        break;
    case TextPart::NEWLINE:
        m_base_width = 0;
        m_ruby_width = 0;
        m_part_width = 0;
        break;
    }
    SelectObject(dc, hFontOld);
}

/////////////////////////////////////////////////////////////////////////////
// TextRun - テキストの連続(ラン)。

/**
 * ランの高さを計測する。
 * @param doc 文書。
 */
void TextRun::UpdateHeight(TextDoc& doc) {
    // ルビがあるか？
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)doc.m_parts.size());
        TextPart& part = doc.m_parts[iPart];
        if (part.m_type == TextPart::RUBY && part.m_ruby_len > 0)
            m_has_ruby = true;
    }

    m_base_height = doc.m_base_height;
    if (m_has_ruby) {
        m_ruby_height = doc.m_ruby_height;
        m_run_height = m_ruby_height + m_base_height;
    } else {
        m_ruby_height = 0;
        m_run_height = m_base_height;
    }
}

/**
 * ランの幅を計測する。
 * @param doc 文書。
 */
void TextRun::UpdateWidth(TextDoc& doc) {
    std::vector<TextPart>& parts = doc.m_parts;
    m_run_width = 0;
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        TextPart& part = parts[iPart];
        part.UpdateWidth(doc, doc.m_hBaseFont, doc.m_hRubyFont);
        m_run_width += part.m_part_width;
    }
}

/////////////////////////////////////////////////////////////////////////////
// TextDoc

/**
 * 段落の選択を更新する
 */
void TextDoc::UpdateSelection() {
    std::vector<TextPart>& parts = m_parts;
    INT iStart = m_selection_start;
    INT iEnd = m_selection_end;
    if (iStart == -1) {
        for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
            parts[iPart].m_selected = false;
        }
    } else {
        if (iEnd == -1)
            iEnd = (INT)parts.size();

        if (iStart > iEnd)
            std::swap(iStart, iEnd);

        for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
            parts[iPart].m_selected = (iStart <= (INT)iPart && (INT)iPart < iEnd);
        }
    }
}

/**
 * 段落を追加する。
 * @param text テキスト文字列。
 */
void TextDoc::_AddPara(const std::wstring& text) {
    size_t ich = m_text.size();
    m_text += text;

    TextPara para;
    para.m_part_index_start = m_parts.size();

    bool has_ruby = false;
    while (ich < m_text.length()) {
        // "{ベーステキスト(ルビテキスト)}"
        if (m_text[ich] == L'{') {
            intptr_t paren_start = m_text.find(L'(', ich);
            if (paren_start != m_text.npos) {
                intptr_t furigana_end = m_text.find(L")}", paren_start);
                if (furigana_end != m_text.npos) {
                    TextPart part;
                    part.m_type = TextPart::RUBY;
                    part.m_start_index = ich;
                    part.m_end_index = furigana_end + 2;
#ifndef NDEBUG // デバッグ時のみ
                    part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
                    part.m_base_index = ich + 1;
                    part.m_base_len = paren_start - (ich + 1);
                    part.m_ruby_index = paren_start + 1;
                    part.m_ruby_len = furigana_end - (paren_start + 1);
                    m_parts.push_back(part);
                    ich = furigana_end + 2;
                    has_ruby = true;
                    continue;
                }
            }
        }

        // "漢字(ふりがな)"
        size_t ich0 = ich; // 漢字の始まり？
        size_t kanji_len = skip_kanji_chars(m_text, ich);
        if (kanji_len > 0) {
            if (ich < m_text.length() && m_text[ich] == L'(') { // 漢字の次に半角の丸カッコがある？
                ++ich;
                size_t ich1 = ich; // フリガナの始まり？
                size_t kana_len = skip_kana_chars(m_text, ich);
                if (kana_len > 0) { // 丸カッコの後にカナがある？
                    size_t ich2 = ich; // フリガナの終わり？
                    if (ich2 < m_text.length() && m_text[ich2] == L')') { // フリガナの次に「丸カッコ閉じる」がある？
                        TextPart part;
                        part.m_type = TextPart::RUBY;
                        part.m_start_index = ich0;
                        part.m_end_index = ich2 + 1;
#ifndef NDEBUG // デバッグ時のみ
                        part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
                        part.m_base_index = ich0;
                        part.m_base_len = (ich1 - 1) - ich0;
                        part.m_ruby_index = ich1;
                        part.m_ruby_len = ich2 - ich1;
                        m_parts.push_back(part);
                        ich = ich2 + 1;
                        has_ruby = true;
                        continue;
                    }
                }
            }
            ich = ich0;
        }

        size_t char_index = ich;
        size_t char_len = skip_one_real_char(m_text, ich);
        TextPart part;
        part.m_type = TextPart::NORMAL;
        part.m_start_index = char_index;
        part.m_end_index = ich;
#ifndef NDEBUG // デバッグ時のみ
        part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
        part.m_base_index = char_index;
        part.m_base_len = char_len;
        part.m_ruby_index = 0;
        part.m_ruby_len = 0;
        m_parts.push_back(part);
    }

    para.m_part_index_end = m_parts.size();
    m_paras.push_back(para);
}

/**
 * テキストを分割する。
 * @param container コンテナ。
 * @param str テキスト文字列。
 * @param chars 分割の区切り文字の集合。
 */
template <typename T_STR_CONTAINER>
inline void
mstr_split(T_STR_CONTAINER& container,
           const typename T_STR_CONTAINER::value_type& str,
           const typename T_STR_CONTAINER::value_type& chars)
{
    container.clear();
    size_t i = 0, k = str.find_first_of(chars);
    while (k != T_STR_CONTAINER::value_type::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

/**
 * テキストを追加する。
 * @param text テキスト文字列。
 */
void TextDoc::AddText(const std::wstring& text) {
    // 改行文字で分割
    std::vector<std::wstring> lines;
    mstr_split(lines, text, std::wstring(L"\n"));

    for (size_t iLine = 0; iLine < lines.size(); ++iLine) {
        std::wstring line = lines[iLine];
        size_t ich = m_text.size() + line.size();
        // 段落を追加
        _AddPara(line);

        if (iLine + 1 != lines.size()) {
            // 段落に含まれない改行文字を追加
            TextPart part;
            part.m_type = TextPart::NEWLINE;
            part.m_start_index = ich;
            part.m_end_index = ich + 1;
#ifndef NDEBUG // デバッグ時のみ
            part.m_text = L"\n";
#endif
            part.m_base_index = ich;
            part.m_base_len = 1;
            part.m_ruby_index = 0;
            part.m_ruby_len = 0;
            m_parts.push_back(part);
            m_text += L"\n";
        }
    }
}

/**
 * パーツの高さを計算する。
 */
void TextDoc::_UpdatePartsHeight() {
    HGDIOBJ hFontOld = SelectObject(m_dc, m_hBaseFont);
    TEXTMETRICW tm;
    GetTextMetricsW(m_dc, &tm);
    m_base_height = tm.tmHeight; // ベーステキストのフォントの高さ
    SelectObject(m_dc, m_hRubyFont);
    GetTextMetricsW(m_dc, &tm);
    m_ruby_height = tm.tmHeight; // ルビテキストのフォントの高さ
    SelectObject(m_dc, hFontOld);
}

/**
 * パーツの幅を計算する。
 */
void TextDoc::_UpdatePartsWidth() {
    for (size_t iPart = 0; iPart < m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        part.UpdateWidth(*this, m_hBaseFont, m_hRubyFont);
    }
}

/**
 * 段落の当たり判定。
 * @param x X座標。
 * @param y Y座標。
 * @return パートのインデックス。
 */
INT TextDoc::HitTest(INT x, INT y) {
    if (m_runs.empty()) {
        _UpdateRuns();
    }

    // 垂直方向
    INT current_y = 0;
    size_t iRun;
    for (iRun = 0; iRun < m_runs.size(); ++iRun) {
        const TextRun& run = m_runs[iRun];
        if (y < current_y + run.m_run_height)
            break;

        current_y += run.m_run_height;
    }

    if (iRun < m_runs.size()) {
        const TextRun& run = m_runs[iRun];
        INT iPart = run.m_part_index_start;

        x -= run.m_delta_x; // 右そろえ、中央そろえの修正分

        // 水平方向
        INT current_x = 0;
        for (; iPart < run.m_part_index_end; ++iPart) {
            const TextPart& part = m_parts[iPart];
            if (x < current_x + part.m_part_width / 2)
                return (INT)iPart;
            current_x += part.m_part_width;
        }

        return (INT)run.m_part_index_end;
    }

    return m_runs.back().m_part_index_end;
}

/**
 * 0個以上のランをする。
 * @return 入植したランの個数。
 */
INT TextDoc::_UpdateRuns() {
    m_runs.clear();

    // パーツの寸法を計算する
    _UpdatePartsHeight();
    _UpdatePartsWidth();

    // 折り返し処理を行ってランを追加していく。ついでに各ランの幅を計算する
    size_t iPart, iPart0 = 0, iRun = 0;
    INT current_x = 0, run_width = 0;
    for (iPart = 0; iPart < m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        INT part_width = part.m_part_width;

        // 最大幅を超えたら、折り返し。
        // TODO: 禁則処理
        if (part.m_text == L"\n" || (m_max_width > 0 && current_x + part_width > m_max_width && current_x != 0)) {
            // ランを追加
            TextRun run;
            run.m_part_index_start = iPart0;
            run.m_part_index_end = iPart;
            run.m_run_width = run_width;
            m_runs.push_back(run);

            iPart0 = iPart + (part.m_text == L"\n");
            current_x = run_width = 0;
        }

        current_x += part_width;
        run_width += part_width;
    }

    // 折り返しの残りのパーツのランを追加
    TextRun run;
    run.m_part_index_start = iPart0;
    run.m_part_index_end = iPart;
    run.m_run_width = run_width;
    m_runs.push_back(run);

    // 各ランの高さを計算する
    m_para_height = 0;
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        TextRun& run = m_runs[iRun];
        run.UpdateHeight(*this);
        m_para_height += run.m_run_height;
    }

    return (INT)m_runs.size();
}

void TextDoc::SetSelection(INT iStart, INT iEnd) {
    m_selection_start = iStart;
    m_selection_end = iEnd;
}

std::wstring TextDoc::GetSelectedText() {
    std::vector<TextPart>& parts = m_parts;
    INT iStart = m_selection_start;
    INT iEnd = m_selection_end;
    if (iStart == -1) {
        return L"";
    } else {
        if (iEnd == -1)
            iEnd = (INT)parts.size();

        if (iStart > iEnd)
            std::swap(iStart, iEnd);

        std::wstring text;
        for (size_t iPart = 0; iPart < parts.size(); ++iPart) {
            if (iStart <= (INT)iPart && (INT)iPart < iEnd) {
                TextPart& part = parts[iPart];
                text += m_text.substr(part.m_base_index, part.m_base_len);
            }
        }
        return text;
    }
}

/**
 * 1個のランを描画する。
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param run 描画したいラン。
 * @param prc 描画する位置とサイズ。計測のみの場合、サイズが変更される。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT。
 */
void TextDoc::_DrawRun(
    HDC dc,
    TextRun& run,
    LPRECT prc,
    UINT flags,
    const COLORREF *colors)
{
    if (!colors)
        colors = get_default_colors();

    if (m_text.length() <= 0 || m_parts.empty()) {
        if (!dc) {
            prc->right = prc->left;
            prc->bottom = prc->top;
        }
        return;
    }

    // dcがNULLの場合、計測用に画面のHDCを使用する
    HDC hdc = dc ? dc : CreateCompatibleDC(NULL);
    if (!hdc) {
        assert(0);
        if (!dc) {
            prc->right = prc->left;
            prc->bottom = prc->top;
        }
        return;
    }

    // 描画/計測の準備
    if (dc) SetBkMode(dc, TRANSPARENT); // 背景モード設定

    if (flags & DT_CENTER)
        run.m_delta_x = (m_max_width - run.m_run_width) / 2;
    else if (flags & DT_RIGHT)
        run.m_delta_x = m_max_width - run.m_run_width;
    else
        run.m_delta_x = 0;

    INT current_x = prc->left + run.m_delta_x;
    const INT base_y = prc->top + run.m_ruby_height; // ベーステキストのY座標

    // しきい値を取得する
    HGDIOBJ hFontOld = SelectObject(hdc, m_hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢i", 2);
    SelectObject(hdc, hFontOld);

    // パーツを順に処理し、計測または描画する
    for (INT iPart = run.m_part_index_start; iPart < run.m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)m_parts.size());
        TextPart& part = m_parts[iPart];

        // 描画対象の長方形
        RECT rc = { current_x, prc->top, current_x + part.m_part_width, prc->top + run.m_run_height };

        HBRUSH hBrush;
        if (part.m_selected) { // 選択状態か？
            SetTextColor(hdc, colors[2]);
            hBrush = CreateSolidBrush(colors[3]);
        } else {
            SetTextColor(hdc, colors[0]);
            hBrush = CreateSolidBrush(colors[1]);
        }
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        if (dc && RectVisible(dc, &rc)) { // 描画すべきか？
            switch (part.m_type) {
            case TextPart::NORMAL:
                // 通常テキストの描画 (ベースラインに描画)
                hFontOld = SelectObject(dc, m_hBaseFont);
                ExtTextOutW(dc, current_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                SelectObject(dc, hFontOld);
                break;
            case TextPart::RUBY:
                {
                    // ベーステキストの配置を決める
                    INT base_start_x = current_x;
                    if (part.m_part_width > part.m_base_width) {
                        base_start_x += (part.m_part_width - part.m_base_width) / 2;
                    }

                    // ルビの配置を決める
                    INT ruby_extra, ruby_start_x = current_x;
                    if (part.m_part_width - part.m_ruby_width > gap_threshold) {
                        // 両端ぞろえ
                        // SetTextCharacterExtraで使用する文字間スペースを設定
                        if (part.m_ruby_len > 1) {
                            ruby_extra = (part.m_part_width - part.m_ruby_width) / (part.m_ruby_len - 1);
                        }
                    } else {
                        // パート内で中央ぞろえ
                        ruby_start_x += (part.m_part_width - part.m_ruby_width) / 2;
                        ruby_extra = 0;
                    }

                    // ベーステキストの描画
                    hFontOld = SelectObject(dc, m_hBaseFont);
                    ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                    SelectObject(dc, hFontOld);

                    // ルビテキストの描画
                    hFontOld = SelectObject(dc, m_hRubyFont);
                    INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                    ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &m_text[part.m_ruby_index], part.m_ruby_len, NULL);
                    SetTextCharacterExtra(dc, old_extra_ruby);
                    SelectObject(dc, hFontOld);
                }
                break;
            case TextPart::NEWLINE:
                break;
            }
        }

        // 次の描画位置に更新
        current_x += part.m_part_width;
    }

    if (!dc) { // 計測なら prc のサイズを更新
        prc->right = current_x;
        prc->bottom = prc->top + run.m_run_height;
    }

    // クリーンアップ
    if (!dc) DeleteDC(hdc);
}

/**
 * 文書を描画する。
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param prc 描画する位置とサイズ。計測のみの場合、サイズが変更される。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT。
 */
void TextDoc::DrawDoc(
    HDC dc,
    LPRECT prc,
    UINT flags,
    const COLORREF *colors)
{
    if (!colors)
        colors = get_default_colors();

    m_max_width = (flags & DT_SINGLELINE) ? -1 : (prc->right - prc->left);

    _UpdateRuns();

    INT current_y = prc->top;
    INT max_run_width = 0;
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        if (iRun > 0)
            current_y += m_line_gap;

        TextRun& run = m_runs[iRun];

        RECT rc = *prc;
        rc.top = current_y;
        rc.bottom = rc.top + run.m_run_height;
        if (dc && RectVisible(dc, &rc))
            _DrawRun(dc, run, &rc, flags, colors);

        if (max_run_width < run.m_run_width)
            max_run_width = run.m_run_width;

        current_y += run.m_run_height;
    }

    if (!dc) {
        prc->right = prc->left + max_run_width;
        prc->bottom = prc->top + current_y;
    }

    m_para_width = max_run_width;
}

/**
 * 文書をクリアする。
 */
void TextDoc::clear() {
    m_text.clear();
    m_parts.clear();
    m_runs.clear();
    m_paras.clear();
}
