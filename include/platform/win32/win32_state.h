#ifndef WIN32_STATE_H
#define WIN32_STATE_H
#include <windows.h>
#include <stdint.h>

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

extern uint32_t g_win32_main_thread_id;
extern WindowHandle g_win32_tray_wnd;
extern WindowHandle g_win32_message_wnd;

PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
PromptState *Platform_GetWin32PromptState(void);

void Platform_SetDrawContextWin32(HDC hdc);
void Platform_GetDrawContextWin32(HDC *out);
void Platform_ReleaseDrawCache(void);

#endif
