#ifndef TYPES_H
#define TYPES_H

#include <windows.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// FORWARD DECLARATIONS - MUST COME FIRST
// ============================================================

// Declare callback types first
typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected);

// THEN declare PromptUser (uses the callback types above)
char* PromptUser(const char* window_list[], const char* window_title, int count, 
                 int height, int width, int item_height, 
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 RenderCallback render_callback);

// ============================================================
// STRUCT DEFINITIONS
// ============================================================

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

#endif // TYPES_H
