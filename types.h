#ifndef TYPES_H
#define TYPES_H

#include <windows.h>

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

#endif
