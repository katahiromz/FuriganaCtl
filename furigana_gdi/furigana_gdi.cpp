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

/////////////////////////////////////////////////////////////////////////////
// TextPart - テキストのパート。

/**
 * パートの幅を計測する。
 * @param para 段落。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextPart::UpdateWidth(TextPara& para, HFONT hBaseFont, HFONT hRubyFont) {
    // 幅の計測
    HDC dc = para.m_dc;
    HGDIOBJ hFontOld = SelectObject(dc, hBaseFont);
    std::wstring& text = para.m_text;
    if (m_type == TextPart::NORMAL) {
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        m_part_width = m_base_width;
        m_ruby_width = 0;
    } else { // TextPart::RUBY
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        SelectObject(dc, hRubyFont);
        m_ruby_width = get_text_width(dc, &text[m_ruby_index], m_ruby_len);
        // ルビブロックの幅は、ベースとルビの幅の大きい方
        m_part_width = std::max(m_base_width, m_ruby_width);
    }
    SelectObject(dc, hFontOld);
}

/////////////////////////////////////////////////////////////////////////////
// TextRun - テキストの連続(ラン)。

/**
 * ランの高さを計測する。
 * @param para 段落。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextRun::UpdateHeight(TextPara& para, HFONT hBaseFont, HFONT hRubyFont) {
    // ルビがあるか？
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)para.m_parts.size());
        TextPart& part = para.m_parts[iPart];
        if (part.m_type == TextPart::RUBY && part.m_ruby_len > 0) {
            m_has_ruby = true;
        }
    }

    m_base_height = para.m_base_height;
    if (m_has_ruby) {
        m_ruby_height = para.m_ruby_height;
        m_run_height = m_ruby_height + m_base_height;
    } else {
        m_ruby_height = 0;
        m_run_height = m_base_height;
    }
}

/**
 * ランの幅を計測する。
 * @param para 段落。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextRun::UpdateWidth(TextPara& para, HFONT hBaseFont, HFONT hRubyFont) {
    std::vector<TextPart>& parts = para.m_parts;
    m_run_width = 0;
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        TextPart& part = parts[iPart];
        part.UpdateWidth(para, hBaseFont, hRubyFont);
        m_run_width += part.m_part_width;
    }
}

/////////////////////////////////////////////////////////////////////////////
// TextPara

/**
 * 段落の選択を更新する
 */
void TextPara::UpdateSelection() {
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
 * 段落をパースする。
 * @param text テキスト文字列。
 * @return ルビがあればtrue、さもなければfalse。
 */
bool TextPara::ParseParts(const std::wstring& text) {
    m_text = text;
    m_parts.clear();

    size_t ich = 0;
    bool has_ruby = false;
    while (ich < text.length()) {
        // "{ベーステキスト(ルビテキスト)}"
        if (text[ich] == L'{') {
            intptr_t paren_start = text.find(L'(', ich);
            if (paren_start != text.npos) {
                intptr_t furigana_end = text.find(L")}", paren_start);
                if (furigana_end != text.npos) {
                    TextPart part;
                    part.m_type = TextPart::RUBY;
                    part.m_start_index = ich;
                    part.m_end_index = furigana_end + 2;
#ifndef NDEBUG // デバッグ時のみ
                    part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
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
        size_t kanji_len = skip_kanji_chars(text, ich);
        if (kanji_len > 0) {
            if (ich < text.length() && text[ich] == L'(') { // 漢字の次に半角の丸カッコがある？
                ++ich;
                size_t ich1 = ich; // フリガナの始まり？
                size_t kana_len = skip_kana_chars(text, ich);
                if (kana_len > 0) { // 丸カッコの後にカナがある？
                    size_t ich2 = ich; // フリガナの終わり？
                    if (ich2 < text.length() && text[ich2] == L')') { // フリガナの次に「丸カッコ閉じる」がある？
                        TextPart part;
                        part.m_type = TextPart::RUBY;
                        part.m_start_index = ich0;
                        part.m_end_index = ich2 + 1;
#ifndef NDEBUG // デバッグ時のみ
                        part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
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
        size_t char_len = skip_one_real_char(text, ich);
        TextPart part;
        part.m_type = TextPart::NORMAL;
        part.m_start_index = char_index;
        part.m_end_index = ich;
#ifndef NDEBUG // デバッグ時のみ
        part.m_text = text.substr(part.m_start_index, part.m_end_index - part.m_start_index);
#endif
        part.m_base_index = char_index;
        part.m_base_len = char_len;
        part.m_ruby_index = 0;
        part.m_ruby_len = 0;
        m_parts.push_back(part);
    }

    return has_ruby;
}

/**
 * パーツの高さを計算する。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextPara::_UpdatePartsHeight(HFONT hBaseFont, HFONT hRubyFont) {
    HGDIOBJ hFontOld = SelectObject(m_dc, hBaseFont);
    TEXTMETRICW tm;
    GetTextMetricsW(m_dc, &tm);
    m_base_height = tm.tmHeight; // ベーステキストのフォントの高さ
    SelectObject(m_dc, hRubyFont);
    GetTextMetricsW(m_dc, &tm);
    m_ruby_height = tm.tmHeight; // ルビテキストのフォントの高さ
    SelectObject(m_dc, hFontOld);
}

/**
 * パーツの幅を計算する。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 */
void TextPara::_UpdatePartsWidth(HFONT hBaseFont, HFONT hRubyFont) {
    for (size_t iPart = 0; iPart < m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        part.UpdateWidth(*this, hBaseFont, hRubyFont);
    }
}

/**
 * 段落の当たり判定。
 * @param x X座標。
 * @param y Y座標。
 * @return パートのインデックス。
 */
INT TextPara::HitTest(INT x, INT y) const {
    if (m_runs.empty()) return 0;

    // 垂直方向
    INT current_y = 0;
    size_t iRun;
    for (iRun = 0; iRun < m_runs.size(); ++iRun) {
        const TextRun& run = m_runs[iRun];
        if (y < current_y + run.m_run_height / 2)
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
            current_x += part.m_part_width;
            if (x < current_x - part.m_part_width / 2)
                return (INT)iPart;
        }

        return (INT)run.m_part_index_end;
    }

    return m_runs.back().m_part_index_end;
}

/**
 * ランを入植する。
 * @param hBaseFont ベーステキストのフォント。
 * @param hRubyFont ルビテキストのフォント。
 * @return 入植したランの個数。
 */
INT TextPara::PopulateRuns(HFONT hBaseFont, HFONT hRubyFont) {
    m_runs.clear();

    // パーツの寸法を計算する
    _UpdatePartsHeight(hBaseFont, hRubyFont);
    _UpdatePartsWidth(hBaseFont, hRubyFont);

    // 折り返し処理を行ってランを追加していく。ついでに各ランの幅を計算する
    size_t iPart, iPart0 = 0, iRun = 0;
    INT current_x = 0, run_width = 0;
    for (iPart = 0; iPart < m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        INT part_width = part.m_part_width;

        // 最大幅を超えたら、折り返し。
        // TODO: 禁則処理
        if (m_max_width > 0 && current_x + part_width > m_max_width && current_x != 0) {
            // ランを追加
            TextRun run;
            run.m_part_index_start = iPart0;
            run.m_part_index_end = iPart;
            run.m_run_width = run_width;
            m_runs.push_back(run);

            iPart0 = iPart;
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
        run.UpdateHeight(*this, hBaseFont, hRubyFont);
        m_para_height += run.m_run_height;
    }

    return (INT)m_runs.size();
}

/**
 * 1個のランを描画する。
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param run 描画したいラン。
 * @param prc 描画する位置とサイズ。計測のみの場合、サイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT。
 */
void TextPara::_DrawRun(
    HDC dc,
    TextRun& run,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags)
{
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
    HGDIOBJ hFontOld = SelectObject(hdc, hBaseFont);
    INT gap_threshold = get_text_width(hdc, L"漢i", 2);
    SelectObject(hdc, hFontOld);

    // パーツを順に処理し、計測または描画する
    for (INT iPart = run.m_part_index_start; iPart < run.m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)m_parts.size());
        TextPart& part = m_parts[iPart];

        // 描画対象の長方形
        RECT rc = { current_x, prc->top, current_x + part.m_part_width, prc->top + run.m_run_height };

        if (part.m_selected) { // 選択状態か？
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }

        if (dc && RectVisible(dc, &rc)) { // 描画すべきか？
            if (part.m_type == TextPart::NORMAL) {
                // 通常テキストの描画 (ベースラインに描画)
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, current_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                SelectObject(dc, hFontOld);
            } else { // TextPart::RUBY
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
                hFontOld = SelectObject(dc, hBaseFont);
                ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &m_text[part.m_base_index], part.m_base_len, NULL);
                SelectObject(dc, hFontOld);

                // ルビテキストの描画
                hFontOld = SelectObject(dc, hRubyFont);
                INT old_extra_ruby = SetTextCharacterExtra(dc, ruby_extra);
                ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &m_text[part.m_ruby_index], part.m_ruby_len, NULL);
                SetTextCharacterExtra(dc, old_extra_ruby);
                SelectObject(dc, hFontOld);
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
 * 1個の段落を描画する。
 * @param dc 描画するときはデバイスコンテキスト。描画せず、計測したいときは NULL。
 * @param prc 描画する位置とサイズ。計測のみの場合、サイズが変更される。
 * @param hBaseFont ベーステキスト（漢字・通常文字）に使用するフォント。
 * @param hRubyFont ルビテキスト（フリガナ）に使用するフォント。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT。
 */
void TextPara::DrawPara(
    HDC dc,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags)
{
    m_max_width = (flags & DT_SINGLELINE) ? -1 : (prc->right - prc->left);

    PopulateRuns(hBaseFont, hRubyFont);

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
            _DrawRun(dc, run, &rc, hBaseFont, hRubyFont, flags);

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
