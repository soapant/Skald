#ifndef TYPES_H
#define TYPES_H

#include <windows.h>
#include <stdbool.h>
#include <string.h>

typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);

char* PromptUser(const char* window_list[], const char* window_title, int count, 
                 int height, int width, int item_height, 
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 RenderCallback render_callback);

char* SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);

char* DisplayMessage(const char* message, int centerX, int centerY, int width, int height,int timeout);

void UpdateTextCaret(HWND hwndEditControl);

typedef struct {
    int index;
    HWND hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    HWND hwnd;
    char title[256];
    char exe[256];
} WindowData;

typedef struct {
    WindowData* items;
    int count;
    int capacity;
} WindowList;

typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;


// ============================================================
// GLOBAL VARIABLE DECLARATIONS
// ============================================================

extern HWND hPopupWnd;
extern HWND hListBox;
extern HWND hEditControl;
extern bool selection_complete;
extern RefreshCallback g_refresh_callback;
extern EnterCallback g_enter_callback;
extern CtrlEnterCallback g_ctrl_enter_callback;
extern TabCallback g_tab_callback;
extern RenderCallback g_render_callback;
extern const char** g_initial_items;
extern int g_initial_count;
extern int g_SelectedIndex;
extern const char* g_title;
extern const char* g_vim_mode;
#define MAX_ITEMS 1000
extern char g_populated_cache[MAX_ITEMS][512];
extern int g_populated_count;
extern int g_view_offset;
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
extern int g_current_window_height;
extern int g_current_window_width;
#define COLOR_BG      RGB(30, 35, 45)   
#define COLOR_TEXT    RGB(255, 255, 255)
#define COLOR_SELECTED_TEXT    RGB(70, 55, 90)
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;

#define GAP 20


#define MAIN_CENTER_X (GetSystemMetrics(SM_CXSCREEN) / 2)
#define MAIN_CENTER_Y (GetSystemMetrics(SM_CYSCREEN) / 2)

#define MSG_LEFT(msg, msg_width, timeout) \
    DisplayMessage((msg), \
                   (960 - 450 - GAP - ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_RIGHT(msg, msg_width, timeout) \
    DisplayMessage((msg), \
                   (960 + 450 + GAP + ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_TOP(msg, msg_height, timeout) \
    DisplayMessage((msg), \
                   960, \
                   (540 - (g_current_window_height / 2) - GAP + ((msg_height) / 2)), \
                   900, (msg_height), (timeout))

#define MSG_BOTTOM(msg, msg_height, timeout) \
    DisplayMessage((msg), \
                   960, \
                   (540 + (g_current_window_height / 2) + GAP - ((msg_height) / 2)), \
                   900, (msg_height), (timeout))

#endif
