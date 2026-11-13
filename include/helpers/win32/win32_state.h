#ifndef WIN32_STATE_H
#define WIN32_STATE_H
#include <windows.h>
#include <stdint.h>

#include <stdbool.h>
#include "core/types.h"

bool win32_is_main_thread(void);
WindowHandle win32_tray_window(void);
bool win32_message_popup_any_open(void);

void win32_popup_request_repaint(HWND hPopup);
void win32_set_prev_fg(HWND h);

PromptState *win32_prompt_state_from_hwnd(HWND hwnd);
void win32_release_draw_cache(void);

void win32_set_draw_context(HDC hdc);
void win32_get_draw_context(HDC *out);

#endif
