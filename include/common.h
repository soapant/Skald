#ifndef COMMON_H
#define COMMON_H

#if defined(_WIN32)
#ifndef PROPID
#define PROPID DWORD
#endif
#endif

#include "core/types.h"
#include "core/hashmap.h"
#include "core/modal.h"
#include "core/context.h"

extern WindowHandle hGlobalTrayWnd;
extern WindowHandle hPopupWnd;
extern WindowHandle hEditControl;
extern WindowHandle hListBox;
extern WindowHandle hDisplayMessage;
extern WindowHandle hSettings;
extern WindowHandle g_hEditPopup;
extern WindowHandle g_hSearchPopup;
extern WindowHandle g_ActiveEditControl;
extern EditPopupData* g_EditData;
#if !defined(_WIN32)
extern Display* g_font_dpy;
#endif


// Deprecated prompt callback macros removed. Use FlintContext_Get()->prompt_callbacks.* or spec->*
// Kept for transition: if you still use g_refresh_callback etc, include this shim:
// #define g_refresh_callback (FlintContext_Get()->prompt_callbacks.refresh)

#if defined(_WIN32)
extern uint32_t g_main_thread_id;
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;
extern HANDLE g_key_event;
extern HHOOK hKeyboardHook;
extern int g_timeout;
extern bool g_waiting_for_key;
extern int g_key_pressed;
extern KeyCallback g_key_callback;
extern bool draw_background;
#endif
extern char output_buffer[256];
extern bool selection_complete;
extern char* g_current_feature;
extern const char* g_title;

extern HashMap* feature_history;
extern HashMap* g_item_index_map;
extern HashMap* history_indices;
extern WinTimer g_timer;
extern char* SelectionData[2048];
extern int SelectionDataCount;

#endif
