#pragma once

// Styles
#define FCS_NOSCROLL 0x4000

// Messages
// FC_SETRUBYRATIO - Set ruby ratio
#define FC_SETRUBYRATIO (WM_USER + 1000)
// FC_SETMARGIN - Set margin
#define FC_SETMARGIN (WM_USER + 1001)
// FC_SETCOLOR - Set color
#define FC_SETCOLOR (WM_USER + 1002)
// FC_SETLINEGAP - Set line gap
#define FC_SETLINEGAP (WM_USER + 1003)
// FC_GETIDEALSIZE - Get ideal size
#define FC_GETIDEALSIZE (WM_USER + 1004)
// FC_SETSEL - Set selection
#define FC_SETSEL (WM_USER + 1005)
// FC_GETSELTEXT - Get selection text
#define FC_GETSELTEXT (WM_USER + 1006)
// FC_GETSEL - Get selection
#define FC_GETSEL (WM_USER + 1007)

// FCN_LOADCONTEXTMENU
#define FCN_LOADCONTEXTMENU (NM_FIRST + 0)
// FCN_CONTEXTMENUACTION
#define FCN_CONTEXTMENUACTION (NM_FIRST + 1)

struct FURIGANA_NOTIFY : NMHDR {
    UINT action_id;
};
