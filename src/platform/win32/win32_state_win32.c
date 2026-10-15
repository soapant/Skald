#include "platform/win32/win32_state.h"

uint32_t g_win32_main_thread_id = 0;
HFONT g_win32_hMainFont = NULL;
HHOOK g_win32_hKeyboardHook = NULL;
HHOOK g_win32_hMouseHook = NULL;
bool g_win32_waiting_for_key = false;
int g_win32_key_pressed = 0;
KeyCallback g_win32_key_callback = NULL;
