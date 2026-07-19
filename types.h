#ifndef TYPES_H
#define TYPES_H

#include <windows.h>
#include <stdbool.h>
#include <string.h>

typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected);

char* PromptUser(const char* window_list[], const char* window_title, int count, 
                 int height, int width, int item_height, 
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 RenderCallback render_callback);

char* SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);

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
#define COLOR_BG      RGB(30, 34, 42)   
#define COLOR_TEXT    RGB(220, 223, 228)
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;
#endif
