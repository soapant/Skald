#ifndef RAW_INPUT_CORE_WIN32_H
#define RAW_INPUT_CORE_WIN32_H
#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include "core/types/keys.h"
#include "core/types/window.h"
#include "config_values.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/hotkey_manager.h"
#include "helpers/helpers_hotkey.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "helpers/helpers_input_state.h"

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam,
                                         LPARAM lParam);

void win32_register_raw_mouse(HWND hwnd);
void win32_handle_raw_mouse_input(LPARAM lParam);

#endif
