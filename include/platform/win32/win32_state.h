#ifndef WIN32_STATE_H
#define WIN32_STATE_H
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

extern uint32_t g_win32_main_thread_id;
extern HFONT g_win32_hMainFont;
extern HHOOK g_win32_hKeyboardHook;
extern HHOOK g_win32_hMouseHook;
extern bool g_win32_waiting_for_key;
extern KeyCallback g_win32_key_callback;
extern PromptState *g_win32_prompt_state;
PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
PromptState *Platform_GetWin32PromptState(void);

void Platform_SetDrawContextWin32(HDC hdc);
void Platform_GetDrawContextWin32(HDC *out);
void Platform_ReleaseDrawCache(void);

#endif
