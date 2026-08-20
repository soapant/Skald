#ifndef SUBCLASSES_H
#define SUBCLASSES_H

#include "core/types.h"

#if defined(_WIN32)
#define WM_DISPLAY_MESSAGE (WM_APP + 2)
#endif

typedef struct {
  const char* message;
  FontHandle hFont;
  int alpha;
} FadeWindowData;

typedef struct {
  const char* message;
  int centerX;
  int centerY;
  int maxWidth;
  int maxHeight;
  int timeout;
} DisplayMessageParams;


#if defined(_WIN32)
LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DisplayMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SearchPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SearchPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void RequestInitialNavigation(int index);
#endif

#endif
