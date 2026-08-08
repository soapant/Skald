#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#ifndef PROPID
#define PROPID DWORD
#endif
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <process.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wincrypt.h>

#include "core/types.h"
#include "core/hashmap.h"
#include "config.h"
#include "core/modal.h"
#include "core/feature_data.h"

// ============================================================
// SHARED GLOBALS (defined in main.c or core .c files)
// ============================================================

extern HWND hGlobalTrayWnd;
extern HWND hPopupWnd;
extern HWND hEditControl;
extern HWND hListBox;
extern HWND hDisplayMessage;
extern HWND hSettings;
extern HWND g_hEditPopup;
extern HWND g_hSearchPopup;
extern HWND g_ActiveEditControl;
extern EditPopupData* g_EditData;

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
extern EditCallback g_edit_callback;
extern FilterCallback g_filter_callback;

extern char* g_current_feature;
extern const char** g_initial_items;
extern int g_initial_count;
extern int g_SelectedIndex;
extern int g_TotalMatches;
extern int g_last_match_count;
extern const char** g_last_items;
extern int g_last_item_count;
extern char g_last_filter[256];
extern int g_last_matches[4000];
extern char g_filter_history[20][256];
extern int g_match_history[20][4000];
extern int g_match_count_history[20];
extern int g_history_index;
extern int g_history_count;
extern int g_matching_indices[4000];
extern char g_populated_cache[MAX_ITEMS][512];
extern int g_populated_count;
extern int g_view_offset;
extern const char* g_title;
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;
extern HashMap* feature_history;
extern HashMap* g_item_index_map;
extern HashMap* history_indices;

extern int g_timeout;
extern bool g_waiting_for_key;
extern int g_key_pressed;
extern WinTimer g_timer;
extern HANDLE g_key_event;

extern HHOOK hKeyboardHook;

// ============================================================
// GLOBAL DATA DEFINED IN HEADERS ORIGINALLY
// ============================================================

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
extern COLORREF DisplayMessageText;
extern COLORREF g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1];
extern bool draw_background;

extern bool mic_muted;

#endif
