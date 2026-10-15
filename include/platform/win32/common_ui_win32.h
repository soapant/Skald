#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include "core/types.h"

HFONT CreateCustomNerdFont(int height, const char* face_name);
void UpdateTextCaret(void);
void CalculateWindowMetrics(WindowLayout* layout, int height, int width, int item_height);
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
WindowHandle FindWindowByPartialTitle(const char* partialTitle);
COLORREF LerpColor(COLORREF a, COLORREF b, float t);

#endif
