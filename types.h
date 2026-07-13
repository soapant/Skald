#ifndef TYPES_H
#define TYPES_H

#include <windows.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
    int index;
    HWND hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    HWND hwnd;
    char title[256]; // Dynamic allocation is better, but a fixed buffer keeps it simple
    char exe[256];
} WindowData;

// Structure to pass your list into the callback
typedef struct {
    WindowData* items;
    int count;
    int capacity;
} WindowList;
// Struct to keep metric management clean
typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;

typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected);

char* PromptUser(const char* window_list[], const char* window_title,int count, int height, int width,int item_height, 
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 RenderCallback render_callback);
extern HWND hPopupWnd;
extern HWND hListBox;
extern HWND hEditControl;
extern bool selection_complete;
#endif
