#ifndef SUBCLASSES_H
#define SUBCLASSES_H
#include <wincrypt.h>
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <process.h>
#include <shlwapi.h> 
#include "helpers.h"
#include "core/modal.h"
#include "core/types.h"
#include "config.h"
#include "core/common_ui.h"
#include "core/number_shortcut.h"
#include "core/feature_data.h"

extern int g_refresh_count;
extern COLORREF DisplayMessageText;
extern COLORREF g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1];
extern bool draw_background;

typedef struct {
  const char* message;
    HFONT hFont;
    int alpha;
} FadeWindowData;

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DisplayMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SearchPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SearchPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif
