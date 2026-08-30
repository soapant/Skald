#include "common.h"
#include "core/number_shortcut.h"
#include "core/common_ui.h"
#include "helpers.h"

extern char output_buffer[256];

static struct {
    int first_digit;
    int state;
    uint32_t timer_id;
    WindowHandle hEditControl;
    WindowHandle hListBox;
    ListBoxInfo* lb;
    bool close_popup;
} g_num_shortcut_state = {0, 0, 0, WH_NULL, WH_NULL, NULL, true};

static void ExecuteNumberShortcut(int number, bool close_popup) {
    if (WH_TO_HWND(g_num_shortcut_state.hListBox) == NULL) return;
    if (!g_num_shortcut_state.lb) return;

    int index = number - 1;
    int count = UI_GetCount(g_num_shortcut_state.lb, g_num_shortcut_state.hListBox);

    if (index >= 0 && index < count) {
        UI_SetSelection(g_num_shortcut_state.lb, g_num_shortcut_state.hListBox, index);

        const char* selected_text = g_num_shortcut_state.lb->populated_cache[index];

        strncpy(output_buffer, selected_text, sizeof(output_buffer) - 1);
        output_buffer[sizeof(output_buffer) - 1] = '\0';
        SaveFeatureHistory(output_buffer);
        if (g_enter_callback != NULL) {
            g_enter_callback(output_buffer, g_num_shortcut_state.lb);
            selection_complete = true;
        }
        printf("[NUM-SHORTCUT] Selected item %d: %s (close=%d)\n",
               index + 1, output_buffer, close_popup);
    } else {
        printf("[NUM-SHORTCUT] Item %d doesn't exist (only %d items)\n",
               index + 1, count);
    }

    g_num_shortcut_state.state = 0;
    g_num_shortcut_state.first_digit = 0;
}

#if defined(_WIN32)
static void CALLBACK NumberShortcutTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_num_shortcut_state.state == 1 && g_num_shortcut_state.first_digit >= 0) {
        int number = g_num_shortcut_state.first_digit;
        if (number == 0) {
            number = 10;
        }
        printf("[NUM-SHORTCUT] Timer expired - executing single digit: %d\n", number);
        ExecuteNumberShortcut(number, g_num_shortcut_state.close_popup);
    }
    g_num_shortcut_state.state = 0;
    g_num_shortcut_state.first_digit = 0;
}
#endif

bool HandleNumberShortcut(int wParam, WindowHandle hListBox, WindowHandle hEditControl, ListBoxInfo* lb) {
    int digit = wParam - '0';
    int count = UI_GetCount(lb, hListBox);
    if (count == 0) return false;
    
    g_num_shortcut_state.hListBox = hListBox;
    g_num_shortcut_state.hEditControl = hEditControl;
    g_num_shortcut_state.lb = lb;
    
    if (g_num_shortcut_state.state == 0) {
        g_num_shortcut_state.first_digit = digit;
        g_num_shortcut_state.state = 1;
#if defined(_WIN32)
        if (g_num_shortcut_state.timer_id != 0) {
            KillTimer(NULL, g_num_shortcut_state.timer_id);
            g_num_shortcut_state.timer_id = 0;
        }
        g_num_shortcut_state.timer_id = SetTimer(NULL, 0, NUMBER_SHORTCUT_DELAY_MS, NumberShortcutTimerProc);
#endif
        printf("[NUM-SHORTCUT] Waiting for second digit... (first=%d)\n", digit);
        return true;
    } else if (g_num_shortcut_state.state == 1) {
        int combined_number = (g_num_shortcut_state.first_digit * 10) + digit;
        
#if defined(_WIN32)
        if (g_num_shortcut_state.timer_id != 0) {
            KillTimer(NULL, g_num_shortcut_state.timer_id);
            g_num_shortcut_state.timer_id = 0;
        }
#endif
        printf("[NUM-SHORTCUT] Two-digit number: %d\n", combined_number);
        ExecuteNumberShortcut(combined_number, g_num_shortcut_state.close_popup);
        return true;
    }
    
    return false;
}

void ResetNumberShortcutState(void) {
#if defined(_WIN32)
    if (g_num_shortcut_state.timer_id != 0) {
        KillTimer(NULL, g_num_shortcut_state.timer_id);
        g_num_shortcut_state.timer_id = 0;
    }
#endif
    g_num_shortcut_state.state = 0;
    g_num_shortcut_state.first_digit = 0;
    g_num_shortcut_state.hListBox = WH_NULL;
    g_num_shortcut_state.hEditControl = WH_NULL;
    g_num_shortcut_state.lb = NULL;
}
