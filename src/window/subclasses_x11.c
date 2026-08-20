#include "common.h"
#include "core/common_ui.h"
#include "core/feature_data.h"
#include "core/modal.h"
#include "core/number_shortcut.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>

static int Linux_KeysymToWindowsVK(KeySym keysym) {
    if (keysym >= XK_a && keysym <= XK_z) return 'A' + (keysym - XK_a);
    if (keysym >= XK_A && keysym <= XK_Z) return 'A' + (keysym - XK_A);
    if (keysym >= XK_0 && keysym <= XK_9) return '0' + (keysym - XK_0);
    if (keysym >= XK_F1 && keysym <= XK_F12) return VK_F1 + (keysym - XK_F1);

    switch (keysym) {
        case XK_Escape: return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter: return VK_RETURN;
        case XK_Tab: return VK_TAB;
        case XK_BackSpace: return VK_BACK;
        case XK_Up: return VK_UP;
        case XK_Down: return VK_DOWN;
        case XK_Left: return VK_LEFT;
        case XK_Right: return VK_RIGHT;
        case XK_Delete: return VK_DELETE;
        case XK_Home: return VK_HOME;
        case XK_End: return VK_END;
        case XK_Page_Up: return VK_PRIOR;
        case XK_Page_Down: return VK_NEXT;
        case XK_space: return VK_SPACE;
        case XK_Control_L: return VK_LCONTROL;
        case XK_Control_R: return VK_RCONTROL;
        case XK_Shift_L: return VK_LSHIFT;
        case XK_Shift_R: return VK_RSHIFT;
        case XK_Alt_L: return VK_LMENU;
        case XK_Alt_R: return VK_RMENU;
        case XK_comma: return VK_OEM_COMMA;
        case XK_period: return VK_OEM_PERIOD;
        case XK_semicolon: return VK_OEM_1;
        case XK_slash: return VK_OEM_2;
        case XK_apostrophe: return VK_OEM_7;
        case XK_bracketleft: return VK_OEM_4;
        case XK_bracketright: return VK_OEM_6;
        case XK_grave: return VK_OEM_3;
        case XK_minus: return VK_OEM_MINUS;
        case XK_equal: return VK_OEM_PLUS;
        default: return 0;
    }
}

bool Linux_HandlePromptKey(const XEvent* ev) {
    if (ev->type != KeyPress) return false;

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed = (ev->xkey.state & ControlMask) != 0;
    bool shift_pressed = (ev->xkey.state & ShiftMask) != 0;

    if (wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
        wParam == VK_LMENU || wParam == VK_RMENU) {
        return true;
    }

    if (wParam >= '0' && wParam <= '9') {
        if (ctrl_pressed) {
            HandleNumberShortcut(wParam, hListBox, hEditControl);
        }
        return true;
    }

    if (!ctrl_pressed) {
        if (wParam == VK_UP) {
            UI_NavigateUp();
            return true;
        }
        if (wParam == VK_DOWN) {
            UI_NavigateDown();
            return true;
        }
    }

    if (wParam == VK_RETURN) {
        struct feature_data* data = GetFeatureData(g_current_feature);
        output_buffer[0] = '\0';
        if (UI_IsListBoxSelected()) {
            strncpy(output_buffer, g_populated_cache[g_SelectedIndex], sizeof(output_buffer) - 1);
        }
        SaveFeatureHistory(output_buffer);
        if (ctrl_pressed) {
            AddSelectionToData(output_buffer, g_current_feature, data);
            if (g_ctrl_enter_callback != NULL) {
                g_ctrl_enter_callback(output_buffer);
            }
        } else {
            AddSelectionToData(output_buffer, g_current_feature, data);
            if (g_enter_callback != NULL) {
                g_enter_callback(output_buffer);
            }
            selection_complete = true;
        }
        return true;
    }

    if (wParam == VK_ESCAPE) {
        output_buffer[0] = '\0';
        selection_complete = true;
        return true;
    }

    if (wParam == VK_TAB) {
        struct feature_data* data = GetFeatureData(g_current_feature);
        if (g_tab_callback != NULL) {
            g_tab_callback();
        }
        for (int i = 0; i < data->selection_count; ++i) {
            printf("%s \n", data->last_selections[i]);
        }
        return true;
    }

    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            UI_SetText(hEditControl, (char*)"");
            UI_Redraw(hListBox);
        }
        return true;
    }

    if (wParam == EDIT_TEXT_KEY) {
        output_buffer[0] = '\0';
        if (UI_IsListBoxSelected()) {
            strncpy(output_buffer, g_populated_cache[g_SelectedIndex], sizeof(output_buffer) - 1);
            if ((ctrl_pressed && IsInsertMode()) || IsNormalMode()) {
                if (g_edit_callback != NULL) {
                    g_edit_callback(output_buffer);
                }
                return true;
            }
        }
    }

    if (wParam == COPY_SELECTION_KEY) {
        if (shift_pressed) {
            if (UI_IsListBoxSelected()) {
                strncpy(output_buffer, g_populated_cache[g_SelectedIndex], sizeof(output_buffer) - 1);
                output_buffer[sizeof(output_buffer) - 1] = '\0';
            }
            if (g_copy_callback != NULL) {
                g_copy_callback(output_buffer, UI_LookupTrueIndex(output_buffer), 0);
            } else {
                Platform_CopyToClipboard(output_buffer);
            }
            return true;
        } else if (ctrl_pressed) {
            if (UI_IsListBoxSelected()) {
                strncpy(output_buffer, g_populated_cache[g_SelectedIndex], sizeof(output_buffer) - 1);
                output_buffer[sizeof(output_buffer) - 1] = '\0';
            }
            if (g_copy_callback != NULL) {
                g_copy_callback(output_buffer, UI_LookupTrueIndex(output_buffer), 1);
            } else {
                Platform_CopyToClipboard(output_buffer);
            }
            return true;
        }
    }

    if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
            SetCursorToEndOfText();
        }
        return true;
    }

    if (wParam == VK_UP) {
        if (ctrl_pressed) {
            SetCursorToStartOfText();
        }
        return true;
    }

    return false;
}
