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

// ============================================================
// SHARED GLOBALS (defined in main.c or core .c files)
// ============================================================

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
extern Display* g_font_dpy;

extern uint32_t g_main_thread_id;

extern char output_buffer[256];
extern bool selection_complete;
extern bool setting_complete;
extern int region_step;
extern long saved_x1;
extern long saved_y1;
extern long img_x1, img_y1, img_x2, img_y2;
extern int img_region_step;

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
extern const char** g_initial_items;
extern int g_initial_count;
extern const char* g_title;
#if defined(_WIN32)
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;
extern HANDLE g_key_event;
extern HHOOK hKeyboardHook;
#endif
extern HashMap* feature_history;
extern HashMap* g_item_index_map;
extern HashMap* history_indices;

extern int g_timeout;
extern bool g_waiting_for_key;
extern int g_key_pressed;
extern WinTimer g_timer;

extern KeyCallback g_key_callback;
extern FileEntry G_FILE_ENTRIES[];
extern SequenceState g_seq_state;
extern char* SelectionData[2048];
extern int SelectionDataCount;

extern EditorMode g_current_mode;
extern bool g_vim_mode_enabled;
extern const char* g_vim_mode;
extern int last_index;
extern char* selection_feature;

extern int g_refresh_count;
extern ColorRef DisplayMessageText;
extern ColorRef g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1];
extern bool draw_background;

extern bool mic_muted;

#endif
