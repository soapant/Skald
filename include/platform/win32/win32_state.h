#ifndef WIN32_STATE_H
#define WIN32_STATE_H
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

extern uint32_t g_win32_main_thread_id;
extern HFONT g_win32_hMainFont;
extern HHOOK g_win32_hKeyboardHook;
extern int g_win32_timeout;
extern bool g_win32_waiting_for_key;
extern int g_win32_key_pressed;
extern KeyCallback g_win32_key_callback;
extern PromptState* g_win32_prompt_state;

#endif
