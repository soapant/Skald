#ifndef COMMON_H
#define COMMON_H

#if defined(_WIN32)
#ifndef PROPID
#define PROPID DWORD
#endif
#endif

#include <stdbool.h>
#include <assert.h>

#include "core/types.h"
#include "core/hashmap.h"
#include "core/modal.h"

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


extern RefreshCallback g_refresh_callback;
extern EnterCallback g_enter_callback;
extern CtrlEnterCallback g_ctrl_enter_callback;
extern TabCallback g_tab_callback;
extern HelpCallback g_help_callback;
extern CopyCallback g_copy_callback;
extern RenderCallback g_render_callback;
extern AliasCallback  g_alias_callback;
extern EditCallback g_edit_callback;
extern FilterCallback g_filter_callback;

extern char* g_current_feature;
extern const char* g_title;
extern bool selection_complete;

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
extern char* selection_feature;
extern bool draw_background;
extern char output_buffer[256];
#endif
extern HashMap* feature_history;
extern HashMap* g_item_index_map;
extern HashMap* history_indices;

extern WinTimer g_timer;

extern FileEntry G_FILE_ENTRIES[];
extern char* SelectionData[2048];
extern int SelectionDataCount;

extern const char* g_vim_mode;
extern int last_index;

#endif
