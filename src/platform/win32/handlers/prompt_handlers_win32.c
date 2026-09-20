#include <windows.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include <commctrl.h>
#include "config.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/feature_data.h"
#include "core/types.h"
#include "platform/win32/subclasses_win32.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"

bool Win32_HandlePromptKey(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR) {
        if (wParam == 0x08 || wParam == 0x7F) return true;
        return false;
    }
    if (msg != WM_KEYDOWN) return false;

    FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
    bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
    if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) return false;

    ListBoxInfo* lb = GetLBFromChild(hwnd);

    if (!ctrl_pressed) {
        if (wParam == VK_UP || wParam == VK_DOWN) {
            if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) {
                if (wParam == VK_UP) {
                    UI_NavigateUp(lb, FlintContext_Get()->window.hListBox);
                } else {
                    UI_NavigateDown(lb, FlintContext_Get()->window.hListBox);
                }
            }
            return true;
        }
    }

    if (wParam == VK_RETURN) {
        bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000);
        FlintContext_Get()->window.output_buffer[0] = '\0';
        if (UI_IsListBoxSelected(lb)) {
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
            FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
        } else {
            GetWindowTextA(hwnd, FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
        }
        if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
            KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
            DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            FlintContext_Get()->window.hDisplayMessage = WH_NULL;
        }
        if (ctrl_down) {
            bool should_close = false;
            if (FlintContext_Get()->prompt_callbacks.ctrl_enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy) - 1);
                sel_copy[sizeof(sel_copy) - 1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.ctrl_enter(sel_copy, lb);
            } else {
                should_close = false;
            }
            if (should_close) FlintContext_Get()->window.selection_complete = true;
            else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
            return true;
        } else {
            bool should_close = false;
            if (FlintContext_Get()->prompt_callbacks.enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy) - 1);
                sel_copy[sizeof(sel_copy) - 1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.enter(sel_copy, lb);
            } else {
                should_close = true;
            }
            if (should_close) FlintContext_Get()->window.selection_complete = true;
            else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
            return true;
        }
    }

    if (wParam == VK_ESCAPE) {
        FlintContext_Get()->window.output_buffer[0] = '\0';
        FlintContext_Get()->window.selection_complete = true;
        if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
            KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
            DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
        }
        return true;
    }

    if (wParam == VK_TAB) {
        struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.tab != NULL) {
            FlintContext_Get()->prompt_callbacks.tab(lb);
        }
        for (int i = 0; i < data->selection_count; ++i) {
            log_info("%s ", data->last_selections[i]);
        }
        return true;
    }

    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            UI_SetText(FlintContext_Get()->window.hEditControl, "");
            UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            InvalidateRect(hwnd, NULL, TRUE);
            return true;
        } else {
            char buf[512] = {0};
            GetWindowTextA(hwnd, buf, sizeof(buf));
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
            int len = (int)strlen(buf);
            if (selStart != selEnd) {
                memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
                SetWindowTextA(hwnd, buf);
                SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
            } else if (selStart > 0) {
                memmove(buf + selStart - 1, buf + selStart, len - selStart + 1);
                SetWindowTextA(hwnd, buf);
                SendMessageA(hwnd, EM_SETSEL, selStart - 1, selStart - 1);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return true;
        }
    }

    if (wParam == VK_DELETE) {
        char buf[512] = {0};
        GetWindowTextA(hwnd, buf, sizeof(buf));
        DWORD selStart = 0, selEnd = 0;
        SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
        int len = (int)strlen(buf);
        if (selStart != selEnd) {
            memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        } else if (selStart < (DWORD)len) {
            memmove(buf + selStart, buf + selStart + 1, len - selStart);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return true;
    }

    if (wParam == COPY_SELECTION_KEY) {
        bool copy_ctrl = (GetKeyState(VK_CONTROL) & 0x8000);
        bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
        if (shift_pressed) {
            if (UI_IsListBoxSelected(lb)) {
                if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 0);
            } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
            return true;
        } else if (copy_ctrl) {
            if (UI_IsListBoxSelected(lb)) {
                if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 1);
            } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
            return true;
        }
        return false;
    }

    if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
            char text[256];
            UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, text, sizeof(text));
            UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, strlen(text));
        }
        return true;
    }

    if (wParam == VK_UP) {
        if (ctrl_pressed) {
            UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, 1);
        }
        return true;
    }

    if (ctrl_pressed) {
        if (wParam == VK_OEM_MINUS) FontSize -= 1;
        if (wParam == VK_OEM_PLUS) FontSize += 1;
        if (wParam == VK_OEM_MINUS || wParam == VK_OEM_PLUS) return true;
    }

    return false;
}
