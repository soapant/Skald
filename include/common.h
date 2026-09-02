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


#define g_refresh_callback (FlintContext_Get()->prompt_callbacks.refresh)
#define g_enter_callback (FlintContext_Get()->prompt_callbacks.enter)
#define g_ctrl_enter_callback (FlintContext_Get()->prompt_callbacks.ctrl_enter)
#define g_tab_callback (FlintContext_Get()->prompt_callbacks.tab)
#define g_help_callback (FlintContext_Get()->prompt_callbacks.help)
#define g_copy_callback (FlintContext_Get()->prompt_callbacks.copy)
#define g_render_callback (FlintContext_Get()->prompt_callbacks.render)
#define g_alias_callback (FlintContext_Get()->prompt_callbacks.alias)
#define g_edit_callback (FlintContext_Get()->prompt_callbacks.edit)
#define g_filter_callback (FlintContext_Get()->prompt_callbacks.filter)

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
#define output_buffer (FlintContext_Get()->window.output_buffer)
#define selection_complete (FlintContext_Get()->window.selection_complete)
#define g_current_feature (FlintContext_Get()->window.current_feature)
#define g_title (FlintContext_Get()->window.title)

extern HashMap* feature_history;
extern HashMap* g_item_index_map;
extern HashMap* history_indices;

extern WinTimer g_timer;

extern FileEntry G_FILE_ENTRIES[];
extern char* SelectionData[2048];
extern int SelectionDataCount;

#endif
