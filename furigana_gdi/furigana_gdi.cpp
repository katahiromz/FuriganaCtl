// furigana_gdi.cpp
/////////////////////////////////////////////////////////////////////////////

#include "furigana_gdi.h"
#include "char_judge.h"
#include <assert.h>

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
    ::OutputDebugStringW(text);
    va_end(va);
#endif
}

/**
 * テキストの幅を計測する。
 * @param dc デバイスコンテキスト。
 * @param text テキスト。
 * @param len テキストの長さ。
 * @return テキストの幅。
 */
static INT get_text_width(HDC dc, LPCWSTR text, size_t len) {
    if (len <= 0) return 0;
    SIZE size = {0};
    ::GetTextExtentPoint32W(dc, text, (INT)len, &size);
    return size.cx;
}

static const COLORREF *get_default_colors() {
    static COLORREF s_colors[4];
    s_colors[0] = ::GetSysColor(COLOR_WINDOWTEXT);
    s_colors[1] = ::GetSysColor(COLOR_WINDOW);
    s_colors[2] = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    s_colors[3] = ::GetSysColor(COLOR_HIGHLIGHT);
    return s_colors;
}

/////////////////////////////////////////////////////////////////////////////
// 禁則処理ヘルパー（C++03対応）

//#define NO_KINSOKU

#ifndef NO_KINSOKU // 禁則処理をするか？

static bool is_kinsoku_head(wchar_t ch) {
    static const wchar_t head[] = L"、。，．)]｝〕〉》」』】〙〗〟’”ゝゞ々ぁぃぅぇぉっゃゅょァィゥェォッャュョヮヵヶー゛゜？！";
    const wchar_t *pch = head;
    while (*pch) {
        if (*pch == ch)
            return true;
        ++pch;
    }
    return false;
}

static bool is_kinsoku_tail(wchar_t ch) {
    static const wchar_t tail[] = L"([｛〔〈《「『【〘〖〝‘“（";
    const wchar_t *pch = tail;
    while (*pch) {
        if (*pch == ch)
            return true;
        ++pch;
    }
    return false;
}

#endif // ndef NO_KINSOKU

/////////////////////////////////////////////////////////////////////////////

/**
 * テキストを分割する。
 * @param container 文字列ベクター。
 * @param str 分割するテキスト文字列。
 * @param chars 分割の区切り文字の集合。
 */
inline void
mstr_split(std::vector<std::wstring>& container,
           const std::wstring& str,
           const std::wstring& chars)
{
    container.clear();
    if (str.empty()) {
        container.push_back(L"");
        return;
    }
    if (chars.empty()) {
        container.push_back(str);
        return;
    }
    size_t i = 0, k = str.find_first_of(chars);
    while (k != std::wstring::npos) {
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
    HGDIOBJ hFontOld = ::SelectObject(dc, doc.m_hBaseFont);
    std::wstring& text = doc.m_text;
    switch (m_type) {
    case TextPart::NORMAL:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        m_part_width = m_base_width;
        m_ruby_width = 0;
        break;
    case TextPart::RUBY:
        m_base_width = get_text_width(dc, &text[m_base_index], m_base_len);
        ::SelectObject(dc, doc.m_hRubyFont);
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
    ::SelectObject(dc, hFontOld);
}

/////////////////////////////////////////////////////////////////////////////
// TextRun - テキストの連続(ラン)。

/**
 * ランの高さを計測する。
 * @param doc 文書。
 */
void TextRun::update_height(TextDoc& doc) {
    // ルビがあるか？
    m_has_ruby = false;
    for (INT iPart = m_part_index_start; iPart < m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)doc.m_parts.size());
        TextPart& part = doc.m_parts[iPart];
        if (part.m_type == TextPart::RUBY && part.m_ruby_len > 0) {
            m_has_ruby = true;
            break;
        }
    }

    m_base_height = doc.m_base_height;
    if (m_has_ruby) {
        m_ruby_height = doc.m_ruby_height;
        m_run_height = m_ruby_height + m_base_height;
    } else {
        m_ruby_height = 0;
        m_run_height = m_base_height;
    }

    if (m_run_height < m_base_height) {
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
 * 「汚い」フラグを立てる。このフラグが立っているときは、update_runs()が必要。
 */
void TextDoc::set_dirty() {
    m_layout_dirty = true;
}

/**
 * フォントをセットして汚いフラグを立てる。
 * @param hBaseFont ベーステキストのフォント。弱い参照。
 * @param hRubyFont ルビテキストのフォント。弱い参照。
 */
void TextDoc::set_fonts(HFONT hBaseFont, HFONT hRubyFont) {
    m_hBaseFont = hBaseFont;
    m_hRubyFont = hRubyFont;

    // しきい値を取得する（計測と描画の両方で必要）
    HGDIOBJ hFontOldForGap = ::SelectObject(m_dc, m_hBaseFont);
    m_gap_threshold = get_text_width(m_dc, L"漢i", 2);
    ::SelectObject(m_dc, hFontOldForGap);

    set_dirty();
}

/**
 * 段落を追加する。
 * @param text コンパウンド テキスト文字列。
 */
void TextDoc::_add_para(const std::wstring& text) {
    size_t ich = m_text.size();
    m_text += text;

    TextPara para;
    para.m_part_index_start = (INT)m_parts.size();

    bool has_ruby = false;
    while (ich < m_text.length()) {
        if (m_text[ich] == L'{') {
            // "{}"
            if (ich + 1 < m_text.length() && m_text[ich + 1] == L'}') {
                ich += 2;
                continue;
            }

            // "{ベーステキスト(ルビテキスト)}"
            intptr_t newline = m_text.find(L"\n", ich);
            intptr_t furigana_end = m_text.find(L")}", ich);
            if (furigana_end != m_text.npos && (newline == m_text.npos || newline > furigana_end)) {
                intptr_t paren_start = m_text.rfind(L'(', furigana_end);
                if (paren_start != m_text.npos) {
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

        // 英単語なら、ワードラップのため、単語ごとパートにする。
        if (is_ascii_word_char(m_text[ich])) {
            size_t start = ich;
            INT end = find_word_boundary(m_text, (INT)ich, (INT)m_text.size(), +1);
            ich = end;

            TextPart part;
            part.m_type = TextPart::NORMAL;
            part.m_start_index = start;
            part.m_end_index = ich;
            part.m_text = m_text.substr(start, end - start);
            part.m_base_index = start;
            part.m_base_len = end - start;
            part.m_ruby_index = 0;
            part.m_ruby_len = 0;
            m_parts.push_back(part);
            continue;
        }

        if (m_text[ich] == L'\n') { // 改行文字を検出した場合
            TextPart part;
            part.m_type = TextPart::NEWLINE;
            part.m_start_index = ich;
            ++ich;
            part.m_end_index = ich;
            part.m_text = L"\n";
            part.m_base_index = part.m_start_index;
            part.m_base_len = 1;
            part.m_ruby_index = 0;
            part.m_ruby_len = 0;
            m_parts.push_back(part);
            continue;
        }

        // その他は一文字ずつ
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

    para.m_part_index_end = (INT)m_parts.size();
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
    m_max_width = ((flags & DT_SINGLELINE) && !(flags & (DT_RIGHT | DT_CENTER))) ? MAXLONG : layout_width;

    ensure_layout(flags);

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
        if ((run.m_part_index_start <= iPart && iPart < run.m_part_index_end) ||
             (run.m_part_index_start == run.m_part_index_end && run.m_part_index_start == iPart))
        {
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
    set_dirty();

    m_base_height = 0;
    m_ruby_height = 0;
    m_selection_start = -1;
    m_selection_end = 0;
    m_para_width = 0;
}

/**
 * テキストを追加する。
 * @param text テキスト文字列。
 */
void TextDoc::set_text(const std::wstring& text, UINT flags) {
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

    set_dirty();
}

/**
 * パーツの高さを計算する。
 */
void TextDoc::_update_parts_height() {
    HGDIOBJ hFontOld = ::SelectObject(m_dc, m_hBaseFont);
    TEXTMETRICW tm;
    ::GetTextMetricsW(m_dc, &tm);
    m_base_height = tm.tmHeight; // ベーステキストのフォントの高さ
    SelectObject(m_dc, m_hRubyFont);
    ::GetTextMetricsW(m_dc, &tm);
    m_ruby_height = tm.tmHeight; // ルビテキストのフォントの高さ
    ::SelectObject(m_dc, hFontOld);
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
INT TextDoc::hit_test(INT x, INT y, UINT flags) {
    ensure_layout(flags);

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

            // 改行文字の場合は、このランの終端として扱う
            if (part.m_type == TextPart::NEWLINE)
                return iPart;

            // パートの中央より左か？
            if (x < current_x + part.m_part_width / 2)
                return (INT)iPart;

            current_x += part.m_part_width;
        }

        return (INT)run.m_part_index_end;
    }

    return m_runs.back().m_part_index_end;
}

/**
 * 選択領域を表すインデックス区間を正規化する。関数は引数値を変更する。
 * @param iStart 開始のパートインデックスまたは-1。
 * @param iEnd 終了のパートインデックスまたは-1。
 */
void TextDoc::get_normalized_selection(INT& iStart, INT& iEnd) {
    if (iStart == -1) // 選択なし
        return;
    if (iStart == 0 && iEnd == -1) { // すべて選択
        iEnd = (INT)m_parts.size();
        return;
    }
    // それ以外の選択
    INT start = min(iStart, iEnd), end = max(iStart, iEnd);
    iStart = start;
    iEnd = end;
    assert(iStart <= iEnd);
    assert(iStart >= 0);
    assert(iEnd >= 0);
}

/**
 * 選択テキストを取得する。
 * @return 取得したテキスト文字列。
 */
std::wstring TextDoc::get_selection_text() {
    INT start = m_selection_start;
    INT end = m_selection_end;
    get_normalized_selection(start, end);
    if (start == -1 || end == -1) return L"";

    std::wstring text;
    for (INT iPart = start; iPart < end; ++iPart) {
        TextPart& part = m_parts[iPart];
        text += m_text.substr(part.m_base_index, part.m_base_len);
    }

    return text;
}

/**
 * 1個以上のランを更新する。
 * @return 作成されたランの個数。
 */
INT TextDoc::update_runs(UINT flags) {
    m_runs.clear();

    // パーツの寸法を計算する
    _update_parts_height();
    _update_parts_width();

    INT iPart0 = 0; // 現在のランの開始パートインデックス
    INT current_x = 0; // 現在のランの幅

    // パーツを全て処理するまでループ
    for (INT iPart = 0; iPart < (INT)m_parts.size(); ++iPart) {
        TextPart& part = m_parts[iPart];
        INT part_width = part.m_part_width;

        // 1. 改行文字 (TextPart::NEWLINE)
        if (part.m_type == TextPart::NEWLINE) {
            TextRun run;
            run.m_part_index_start = (INT)iPart0;
            run.m_part_index_end = iPart + 1; // 改行文字を含めてランを確定
            run.m_run_width = current_x;
            run.m_max_width = m_max_width;
            m_runs.push_back(run);

            // 次パートは改行の次から開始
            iPart0 = iPart + 1;
            current_x = 0;
            continue;
        }

        // 2. 折り返しが必要か？ (現在のパートを加えると最大幅を超えるか)
        bool wrap = (m_max_width > 0 && current_x + part_width > m_max_width);

        if (wrap) {
            // A) 行頭でのオーバーフロー (current_x == 0):
            //    このパート単独で最大幅を超えている。このパートを単独でランとし、次のパートへ進む
            if (current_x == 0) {
                TextRun run;
                run.m_part_index_start = (INT)iPart0;
                run.m_part_index_end = iPart + 1; // 超過パートを含めて確定
                run.m_run_width = part_width;
                run.m_max_width = m_max_width;
                m_runs.push_back(run);

                // 次の行の開始を現在のパートの次にする
                iPart0 = iPart + 1;
                current_x = 0;
                continue; // for の ++iPart で次のパートに進む
            }

            // B) 行の途中での折り返し (current_x != 0):
            //    現在のパート iPart の手前 (iPart - 1) で改行を試みる

            // iPart は次行の先頭になるべきパート。
            // iBreak は、この行の終端となるパート（exclusive）。
            INT iBreak = iPart;
            INT break_width = current_x;
            bool kinsoku_applied = false;

#ifndef NO_KINSOKU // 禁則処理をするか？
            // 直前パート末尾文字 / 現パート先頭文字を取得
            wchar_t prevCh = 0, nextCh = 0;
            if (iPart > 0) {
                const std::wstring &prevText = m_parts[iPart - 1].m_text;
                if (!prevText.empty())
                    prevCh = prevText[prevText.size() - 1];
            }
            if (!part.m_text.empty())
                nextCh = part.m_text[0];

            // 現行に少なくとも1パートある (iPart - iPart0 >= 1)
            if (iPart > iPart0) {
                INT lastIdx = iPart - 1; // 現行ランの末尾パート
                INT lastWidth = m_parts[lastIdx].m_part_width;

                // 1) 次が行頭禁則文字なら -> 現行の末尾1パートを次行へ移す試み
                if (is_kinsoku_head(nextCh)) {
                    INT next_line_width = lastWidth + part_width; // 次行に入る幅 (末尾 + 現パート)

                    if (next_line_width <= m_max_width) {
                        // 禁則適用: 1つ前のパート (lastIdx) まで巻き戻し、それを次行の先頭にする
                        iBreak = lastIdx;
                        break_width = current_x - lastWidth; // ラン幅を1パート分減らす
                        kinsoku_applied = true;
                    }
                }

                // 2) 直前が行末禁則文字なら -> 同様に末尾1パートを次行へ移す試み (既に kinsoku_applied ならスキップ)
                if (!kinsoku_applied && is_kinsoku_tail(prevCh)) {
                    INT next_line_width = lastWidth + part_width;

                    if (next_line_width <= m_max_width) {
                        // 禁則適用: 1つ前のパート (lastIdx) まで巻き戻し、それを次行の先頭にする
                        iBreak = lastIdx;
                        break_width = current_x - lastWidth;
                        kinsoku_applied = true;
                    }
                }
            }
#endif // ndef NO_KINSOKU

            // 行を確定 (iPart0 から iBreak の直前まで)
            TextRun run;
            run.m_part_index_start = (INT)iPart0;
            run.m_part_index_end = iBreak; // exclusive
            run.m_run_width = break_width;
            run.m_max_width = m_max_width;
            m_runs.push_back(run);

            // 次の行の開始位置を iBreak に設定
            iPart0 = iBreak;

            // iBreak から iPart-1 までの幅 (持ち越されたパートの幅)
            INT carry_over_width = 0;
            for (INT pi = iBreak; pi < iPart; ++pi) {
                carry_over_width += m_parts[pi].m_part_width;
            }
            current_x = carry_over_width;

            // iPart の処理を再開するため、for ループの ++iPart が走る前にインデックスを調整する。
            iPart = iBreak - 1;
            continue; // for の ++iPart で iBreak になり、次回のループで iBreak から処理が再開される
        }

        // 3. 通常の処理 (幅を最大幅を超えない限り加算)
        current_x += part_width;
    }

    // 折り返しの残りのパーツのランを追加
    TextRun run;
    run.m_part_index_start = (INT)iPart0;
    run.m_part_index_end = (INT)m_parts.size();
    run.m_run_width = current_x;
    run.m_max_width = m_max_width;
    m_runs.push_back(run);

    // 各ランの高さを計算する
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        TextRun& run = m_runs[iRun];
        run.update_height(*this);
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
        hBrushBg = ::CreateSolidBrush(colors[1]);
        hBrushSel = ::CreateSolidBrush(colors[3]);
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

    // 選択領域を種痘。
    INT iStart = m_selection_start, iEnd = m_selection_end;
    get_normalized_selection(iStart, iEnd);

    // パーツを順に処理し、計測または描画する
    for (INT iPart = run.m_part_index_start; iPart < run.m_part_index_end; ++iPart) {
        assert(0 <= iPart && iPart < (INT)m_parts.size());
        TextPart& part = m_parts[iPart];

        // 描画対象の長方形（計測用に作るが、描画は dc のときのみ行う）
        RECT rc = { current_x, prc->top, current_x + part.m_part_width, prc->top + run.m_run_height };

        if (dc) {
            // 背景の塗りつぶし（選択状態ごとにブラシを使い分け） — ブラシは再利用
            if (iStart <= iPart && iPart < iEnd) {
                ::SetTextColor(dc, colors[2]);
                ::FillRect(dc, &rc, hBrushSel);
            } else {
                ::SetTextColor(dc, colors[0]);
                ::FillRect(dc, &rc, hBrushBg);
            }

            switch (part.m_type) {
            case TextPart::NORMAL:
                {
                    // 長さゼロ以下は無視。また、パートの最後の'\r'は無視する。
                    INT base_len = (INT)part.m_base_len;
                    if (base_len <= 0)
                        break;
                    size_t base_index = part.m_base_index;
                    if (base_index + base_len - 1 < m_text.length()) {
                        if (m_text[base_index + base_len - 1] == L'\r')
                            --base_len;
                    }

                    HGDIOBJ hOld = ::SelectObject(dc, m_hBaseFont);
                    ::ExtTextOutW(dc, current_x, base_y, 0, NULL, &m_text[base_index], base_len, NULL);
                    ::SelectObject(dc, hOld);
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
                    if (part.m_part_width - part.m_ruby_width > m_gap_threshold) {
                        // 両端ぞろえ
                        if (part.m_ruby_len > 1) {
                            ruby_extra = (part.m_part_width - part.m_ruby_width) / ((INT)part.m_ruby_len - 1);
                        }
                    } else {
                        // パート内で中央ぞろえ
                        ruby_start_x += (part.m_part_width - part.m_ruby_width) / 2;
                        ruby_extra = 0;
                    }

                    // ベーステキストの描画
                    HGDIOBJ hOldBase = ::SelectObject(dc, m_hBaseFont);
                    ::ExtTextOutW(dc, base_start_x, base_y, 0, NULL, &m_text[part.m_base_index], (INT)part.m_base_len, NULL);
                    ::SelectObject(dc, hOldBase);

                    // ルビテキストの描画
                    HGDIOBJ hOldRuby = ::SelectObject(dc, m_hRubyFont);
                    INT old_extra_ruby = ::SetTextCharacterExtra(dc, ruby_extra);
                    ::ExtTextOutW(dc, ruby_start_x, prc->top, 0, NULL, &m_text[part.m_ruby_index], (INT)part.m_ruby_len, NULL);
                    ::SetTextCharacterExtra(dc, old_extra_ruby);
                    ::SelectObject(dc, hOldRuby);
                }
                break;
            case TextPart::NEWLINE:
                break;
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
    if (hBrushBg) ::DeleteObject(hBrushBg);
    if (hBrushSel) ::DeleteObject(hBrushSel);
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

    ensure_layout(flags);

    INT current_y = prc->top;
    INT max_run_width = 0;
    for (size_t iRun = 0; iRun < m_runs.size(); ++iRun) {
        if (iRun > 0)
            current_y += m_line_gap;

        TextRun& run = m_runs[iRun];

        RECT rc = *prc;
        rc.top = current_y;
        rc.bottom = rc.top + run.m_run_height;
        _draw_run(dc, run, &rc, flags, colors);

        if (max_run_width < run.m_run_width)
            max_run_width = run.m_run_width;

        current_y += run.m_run_height;
    }

    if (!dc) {
        prc->right = prc->left + max_run_width;
        prc->bottom = current_y;
    }

    m_para_width = max_run_width;
}

/**
 * 文書の理想的なサイズを取得する。
 * @param prc クライアント領域のRECT構造体へのポインタ。関数はサイズを変更する。
 * @param flags 次のフラグを使用可能: DT_LEFT, DT_CENTER, DT_RIGHT, DT_SINGLELINE。
 */
void TextDoc::get_ideal_size(LPRECT prc, UINT flags) {
    assert(prc);
    draw_doc(NULL, prc, flags, NULL);
}

void TextDoc::ensure_layout(UINT flags) {
    if (m_layout_dirty) {
        DPRINTF(L"[ensure_layout] updating runs with flags: 0x%X\n", flags);
        update_runs(flags);
        m_layout_dirty = false;
        DPRINTF(L"[ensure_layout] runs count: %d\n", (INT)m_runs.size());
    }
}
