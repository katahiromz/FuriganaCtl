// furigana_gdi.cpp
/////////////////////////////////////////////////////////////////////////////

#include "furigana_gdi.h"
#include "char_judge.h"
#include <assert.h>

/**
 * テキストの幅を計測する。
 * @param dc デバイスコンテキスト。
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

/**
 * テキストを分割する。
 * @param container コンテナ。
 * @param str テキスト文字列。
 * @param chars 分割の区切り文字の集合。
 */
inline void
mstr_split(std::vector<std::wstring>& container,
           const std::wstring& str,
           const std::wstring& chars)
{
    container.clear();
    size_t i = 0, k = str.find_first_of(chars);
    while (k != std::wstring::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

/////////////////////////////////////////////////////////////////////////////
// TextPart - テキストのパート。

/**
 * パートの幅を計測する。
 * @param doc 文書。
 */
void TextPart::update_width(TextDoc& doc) {
    // 幅の計測
    HDC dc = doc.m_dc;
    HGDIOBJ hFontOld = SelectObject(dc, doc.m_hBaseFont);
    std::wstring& text = doc.m_text;
    switch (m_type) {
    case TextPart::NORMAL:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        m_part_width = m_base_width;
        m_ruby_width = 0;
        break;
    case TextPart::RUBY:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        SelectObject(dc, doc.m_hRubyFont);
        m_ruby_width = get_text_width(dc, &text[m_ruby_index], m_ruby_len);
        // ルビブロックの幅は、ベースとルビの幅の大きい方
        m_part_width = max(m_base_width, m_ruby_width);
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
void TextRun::update_height(TextDoc& doc) {
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
void TextRun::update_width(TextDoc& doc) {
    std::vector<TextPart>& parts = doc.m_parts;
    m_run_width = 0;
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        TextPart& part = parts[iPart];
        part.update_width(doc);
        m_run_width += part.m_part_width;
    }
}

/////////////////////////////////////////////////////////////////////////////
// TextDoc

/**
 * 段落の選択を更新する
 */
void TextDoc::update_selection() {
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
void TextDoc::_add_para(const std::wstring& text) {
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
                    part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
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
                        part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
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
        part.m_text = m_text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
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
 * パートのインデックスから座標を求める。
 * 返す座標は layout の起点 (0,0) に対する相対座標です（draw_doc の prc->top/left を 0 と見なしたとき）。
 * @param iPart パートのインデックス。
 * @param layout_width 折り返しを行う場合の幅（pixels）。DT_SINGLELINE のときは無視されます。
 * @param ppt 座標を受け取るPOINT構造体へのポインタ。
 * @param flags draw_doc と同じフラグ（DT_SINGLELINE, DT_CENTER, DT_RIGHT）。
 * @return 成功すれば true、失敗すれば false。
 */
bool TextDoc::get_part_position(INT iPart, INT layout_width, LPPOINT ppt, UINT flags) {
    if (!ppt) return false;
    if (iPart < 0) iPart = 0;
    if (iPart >= (INT)m_parts.size()) iPart = (INT)m_parts.size();

    // m_max_width を設定してランを更新
    m_max_width = (flags & DT_SINGLELINE) ? MAXLONG : layout_width;
    update_runs();

    // ランごとの m_delta_x を計算（draw_doc と同じ方式）
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        TextRun& run = m_runs[iRun];
        if (flags & DT_CENTER)
            run.m_delta_x = (m_max_width - run.m_run_width) / 2;
        else if (flags & DT_RIGHT)
            run.m_delta_x = m_max_width - run.m_run_width;
        else
            run.m_delta_x = 0;
    }

    if (iPart == 0) {
        ppt->x = m_runs.empty() ? 0 : m_runs[0].m_delta_x;
        ppt->y = 0;
        return true;
    }

    // ランを巡回して該当するパートを見つける
    INT current_y = 0;
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        TextRun& run = m_runs[iRun];
        if (iRun > 0)
            current_y += m_line_gap;

        // run の vertical span: [current_y, current_y + run.m_run_height)
        if (iPart >= run.m_part_index_start && iPart < run.m_part_index_end) {
            // 水平方向の位置を計算
            INT current_x = 0;
            for (INT pi = run.m_part_index_start; pi < iPart; ++pi) {
                current_x += m_parts[pi].m_part_width;
            }

            ppt->x = current_x + run.m_delta_x;
            ppt->y = current_y;
            return true;
        }

        current_y += run.m_run_height;
    }

    ppt->x = 0;
    ppt->y = current_y;
    return true;
}

/**
 * 選択位置を設定する。
 * @param iStart パートの開始インデックス。
 * @param iEnd パートの終了インデックス。
 */
void TextDoc::set_selection(INT iStart, INT iEnd) {
    m_selection_start = iStart;
    m_selection_end = iEnd;
}

/**
 * パートの高さを取得する。
 * @param iPart パートのインデックス。
 */
INT TextDoc::get_part_height(INT iPart) {
    if (iPart < 0 || iPart >= (INT)m_parts.size())
        return 0;

    return m_base_height + (m_parts[iPart].has_ruby() ? m_ruby_height : 0);
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

/**
 * テキストを追加する。
 * @param text テキスト文字列。
 */
void TextDoc::add_text(const std::wstring& text) {
    // 改行文字で分割
    std::vector<std::wstring> lines;
    mstr_split(lines, text, std::wstring(L"\n"));

    for (size_t iLine = 0; iLine < lines.size(); ++iLine) {
        std::wstring line = lines[iLine];
        size_t ich = m_text.size() + line.size();
        // 段落を追加
        _add_para(line);

        if (iLine + 1 != lines.size()) {
            // 段落に含まれない改行文字を追加
            TextPart part;
            part.m_type = TextPart::NEWLINE;
            part.m_start_index = ich;
            part.m_end_index = ich + 1;
            part.m_text = L"\n";
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
void TextDoc::_update_parts_height() {
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
void TextDoc::_update_parts_width() {
    for (size_t iPart = 0; iPart < m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        part.update_width(*this);
    }
}

/**
 * 段落の当たり判定。
 * @param x X座標。
 * @param y Y座標。
 * @return パートのインデックス。
 */
INT TextDoc::hit_test(INT x, INT y) {
    if (m_runs.empty())
        update_runs();

    if (m_runs.empty() || y < 0) return 0;

    // 垂直方向
    INT current_y = 0;
    size_t iRun;
    for (iRun = 0; iRun < m_runs.size(); ++iRun) {
        if (iRun > 0) {
            current_y += m_line_gap;
        }

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
 * 選択テキストを取得する。
 * @return 取得したテキスト文字列。
 */
std::wstring TextDoc::get_selection_text() {
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
 * 0個以上のランを更新する。
 * @return 入植したランの個数。
 */
INT TextDoc::update_runs() {
    m_runs.clear();

    // パーツの寸法を計算する
    _update_parts_height();
    _update_parts_width();

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
        run.update_height(*this);
        m_para_height += run.m_run_height;
    }

    // ラン間の行間を加算する（ランが複数ある場合）
    if (m_runs.size() > 1) {
        m_para_height += m_line_gap * (INT)(m_runs.size() - 1);
    }

    return (INT)m_runs.size();
}

/**
 * 1個のランを描画する。
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param run ラン。
 * @param prc 描画する位置とサイズ。計測のみの場合、サイズが変更される。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT, DT_SINGLELINE。
 *
 * 最適化ポイント:
 * - 計測時はメンバの m_dc を使い、Create/DeleteDC を繰り返さない。
 * - 描画で使うブラシは再利用する（各パートで都度 Create/Delete しない）。
 * - 計測パスでは描画呼び出し（FillRect / ExtTextOut 等）をスキップする。
 */
void TextDoc::_draw_run(
    HDC dc,
    TextRun& run,
    LPRECT prc,
    UINT flags,
    const COLORREF *colors)
{
    assert(prc);

    if (!colors)
        colors = get_default_colors();

    if (m_text.length() <= 0 || m_parts.empty()) {
        if (!dc) {
            prc->right = prc->left;
            prc->bottom = prc->top;
        }
        return;
    }

    // 計測用DCはコンストラクタで作った m_dc を使う（コスト削減）
    HDC hdc = dc ? dc : m_dc;

    // 描画時のみ使用するリソース（ブラシ）はここで1回だけ作る
    HBRUSH hBrushBg = NULL;
    HBRUSH hBrushSel = NULL;
    if (dc) {
        hBrushBg = CreateSolidBrush(colors[1]);
        hBrushSel = CreateSolidBrush(colors[3]);
        SetBkMode(dc, TRANSPARENT);
    }

    // delta_x / base_y の計算
    if (flags & DT_CENTER)
        run.m_delta_x = (m_max_width - run.m_run_width) / 2;
    else if (flags & DT_RIGHT)
        run.m_delta_x = m_max_width - run.m_run_width;
    else
        run.m_delta_x = 0;

    INT current_x = prc->left + run.m_delta_x;
    const INT base_y = prc->top + run.m_ruby_height; // ベーステキストのY座標

    // しきい値を取得する（計測と描画の両方で必要）
    HGDIOBJ hFontOldForGap = SelectObject(hdc, m_hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢i", 2);
    SelectObject(hdc, hFontOldForGap);

    // パーツを順に処理し、計測または描画する
    for (INT iPart = run.m_part_index_start; iPart < run.m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)m_parts.size());
        TextPart& part = m_parts[iPart];

        // 描画対象の長方形（計測用に作るが、描画は dc のときのみ行う）
        RECT rc = { current_x, prc->top, current_x + part.m_part_width, prc->top + run.m_run_height };

        if (dc) {
            // 背景の塗りつぶし（選択状態ごとにブラシを使い分け） — ブラシは再利用
            if (part.m_selected) {
                SetTextColor(dc, colors[2]);
                FillRect(dc, &rc, hBrushSel);
            } else {
                SetTextColor(dc, colors[0]);
                FillRect(dc, &rc, hBrushBg);
            }

            if (RectVisible(dc, &rc)) { // 描画すべきか？
                switch (part.m_type) {
                case TextPart::NORMAL:
                    {
                        HGDIOBJ hOld = SelectObject(dc, m_hBaseFont);
                        ExtTextOutW(dc, current_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                        SelectObject(dc, hOld);
                    }
                    break;
                case TextPart::RUBY:
                    {
                        // ベーステキストの配置を決める
                        INT base_start_x = current_x;
                        if (part.m_part_width > part.m_base_width) {
                            base_start_x += (part.m_part_width - part.m_base_width) / 2;
                        }

                        // ルビの配置を決める
                        INT ruby_extra = 0, ruby_start_x = current_x;
                        if (part.m_part_width - part.m_ruby_width > gap_threshold) {
                            // 両端ぞろえ
                            if (part.m_ruby_len > 1) {
                                ruby_extra = (part.m_part_width - part.m_ruby_width) / (part.m_ruby_len - 1);
                            }
                        } else {
                            // パート内で中央ぞろえ
                            ruby_start_x += (part.m_part_width - part.m_ruby_width) / 2;
                            ruby_extra = 0;
                        }

                        // ベーステキストの描画
                        HGDIOBJ hOldBase = SelectObject(dc, m_hBaseFont);
                        ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                        SelectObject(dc, hOldBase);

                        // ルビテキストの描画
                        HGDIOBJ hOldRuby = SelectObject(dc, m_hRubyFont);
                        INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                        ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &m_text[part.m_ruby_index], part.m_ruby_len, NULL);
                        SetTextCharacterExtra(dc, old_extra_ruby);
                        SelectObject(dc, hOldRuby);
                    }
                    break;
                case TextPart::NEWLINE:
                    break;
                }
            }
        } // if (dc)

        // 次の描画位置に更新（計測は常に行う）
        current_x += part.m_part_width;
    }

    if (!dc) { // 計測なら prc のサイズを更新
        prc->right = current_x;
        prc->bottom = prc->top + run.m_run_height;
    }

    // クリーンアップ
    if (hBrushBg) DeleteObject(hBrushBg);
    if (hBrushSel) DeleteObject(hBrushSel);
    // m_dc はコンストラクタで作っているのでここで削除しない
}

/////////////////////////////////////////////////////////////////////////////
// draw_doc / get_ideal_size remain mostly unchanged but benefit from _draw_run improvements

void TextDoc::draw_doc(
    HDC dc,
    LPRECT prc,
    UINT flags,
    const COLORREF *colors)
{
    assert(prc);

    if (!colors)
        colors = get_default_colors();

    m_max_width = (flags & DT_SINGLELINE) ? MAXLONG : (prc->right - prc->left);

    update_runs();

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
            _draw_run(dc, run, &rc, flags, colors);
        else if (!dc)
            _draw_run(NULL, run, &rc, flags, colors);

        if (max_run_width < run.m_run_width)
            max_run_width = run.m_run_width;

        current_y += run.m_run_height;
    }

    if (!dc) {
        prc->right = prc->left + max_run_width;
        prc->bottom = current_y;
    }

    m_para_width = max_run_width;

    // 重要: get_ideal_size()/draw_doc(NULL,...) の呼び出しで
    // m_para_height が正しく更新されるようにする
    if (!dc) {
        m_para_height = prc->bottom - prc->top;
    }
}

/**
 * 文書の理想的なサイズを取得する。
 * @param prc クライアント領域のRECT構造体へのポインタ。関数はサイズを変更する。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT, DT_SINGLELINE。
 */
void TextDoc::get_ideal_size(LPRECT prc, UINT flags)
{
    assert(prc);
    draw_doc(NULL, prc, flags, NULL);
}
