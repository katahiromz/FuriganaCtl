#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include "pstdint.h"

// 一行のフリガナ付きテキストを描画する。
INT DrawFuriganaTextLine(
    HDC dc,
    const std::wstring& compound_text,
    LPRECT prc,
    HFONT hBaseFont,
    HFONT hRubyFont,
    UINT flags);
