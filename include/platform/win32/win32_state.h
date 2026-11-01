#ifndef WIN32_STATE_H
#define WIN32_STATE_H
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
PromptState *Platform_GetWin32PromptState(void);

void Platform_SetDrawContextWin32(HDC hdc);
void Platform_GetDrawContextWin32(HDC *out);
void Platform_ReleaseDrawCache(void);

#endif
