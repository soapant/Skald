#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include "core/types.h"

HFONT CreateCustomNerdFont(int height, const char* face_name);
void UpdateTextCaret(WindowHandle hwndEditControl);
void CalculateWindowMetrics(int count, WindowLayout* layout, int height, int width, int item_height);
void CalculateSettingsWindowMetrics(int count, WindowLayout* layout, int height, int width, int item_height);
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont, const WindowLayout* layout);
void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout);
void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
void FetchExeNameOnDemand(WindowHandle hwnd, char* outExe, size_t maxLen);
WindowHandle FindWindowByPartialTitle(const char* partialTitle);

#endif