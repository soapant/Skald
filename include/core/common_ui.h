#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif
#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "config.h"
#include "core/timer.h"

// ---------------------------------------------------------------------------
// Shared (both platforms)
// ---------------------------------------------------------------------------

extern bool g_add_alias_to_string;
extern char g_alias_display[MAX_ITEMS][512];
extern int g_alias_display_count;
extern bool g_alias_display_active;
void BuildAliasDisplayItems(const char** items, int count);

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter));

ColorRef GetRainbowColor(float t, float s, float v);

static inline bool UI_IsListBoxSelected(ListBoxInfo* lb) {
  return lb && lb->selected_index != -1;
}

void UpdateTextCaret(WindowHandle hwndEditControl);
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);


#if defined(_WIN32)
static inline bool IsPopupActive(WindowHandle hPopupWnd) {
  return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
}
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
HFONT CreateCustomNerdFont(int height, const char* face_name);
void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb);
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout);
void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout);
static inline bool IsWindowValid(WindowHandle h) {
  return WH_TO_HWND(h) != NULL && IsWindow(WH_TO_HWND(h));
}
void Win32_ApplyRoundedCorners(HWND hwnd, int w, int h);
void Win32_DrawBackground(HDC hdc, RECT* rc);
void Win32_DrawSelectionBackground(HDC hdc, RECT* rc, COLORREF base);
#else
#include "core/window_manager.h"
static inline bool IsPopupActive(WindowHandle hPopupWnd) {
  (void)FlintContext_Get()->window.hPopupWnd;
  WindowManager* wm = WindowManager_Get();
  return wm && wm->popup_count > 0;
}
static inline bool IsWindowValid(WindowHandle h) {
  return WH_TO_HWND(h) != NULL;
}
#endif


static inline void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox);

#if defined(_WIN32)
#include "platform/win32/ui_win32.h"
#else
#include "platform/x11/ui_x11.h"
#endif

static inline void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
  FlintContext* _c = FlintContext_Get();
  if (_c->prompt_callbacks.refresh != NULL) {
    char buffer[MaxFeatures];
    UI_GetCurrentText(FlintContext_Get()->window.hEditControl,buffer);
    _c->prompt_callbacks.refresh(buffer,FlintContext_Get()->window.hListBox,lb);
  }
}

#endif
