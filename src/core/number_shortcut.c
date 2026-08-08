#include "common.h"
#include "core/number_shortcut.h"
#include "core/common_ui.h"
#include "helpers.h"

static struct {
    int first_digit;
    int state;
    UINT timer_id;
    HWND hEditControl;
    HWND hListBox;
    bool close_popup;
} g_num_shortcut_state = {0, 0, 0, NULL, NULL, true};

static void ExecuteNumberShortcut(int number, bool close_popup) {
    if (!g_num_shortcut_state.hListBox) return;

    int index = number - 1;
    int count = UI_GetCount(g_num_shortcut_state.hListBox);

    if (index >= 0 && index < count) {
        UI_SetSelection(g_num_shortcut_state.hListBox, index, g_SelectedIndex);

        char* selected_text = g_populated_cache[index];

        strncpy(output_buffer, selected_text, sizeof(output_buffer) - 1);
        output_buffer[sizeof(output_buffer) - 1] = '\0';
        SaveFeatureHistory(output_buffer);
        if (g_enter_callback != NULL) {
            g_enter_callback(output_buffer);
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

static void CALLBACK NumberShortcutTimerProcSelection(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
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

bool HandleNumberShortcut(int wParam, HWND hListBox, HWND hEditControl) {
    if (wParam < '0' || wParam > '9') return false;
    
    if (!hListBox) return false;
    
    int digit = wParam - '0';
    int count = UI_GetCount(hListBox);
    if (count == 0) return false;
    
    g_num_shortcut_state.hListBox = hListBox;
    g_num_shortcut_state.hEditControl = hEditControl;
    
    if (g_num_shortcut_state.state == 0) {
        g_num_shortcut_state.first_digit = digit;
        g_num_shortcut_state.state = 1;
        
        if (g_num_shortcut_state.timer_id != 0) {
            KillTimer(NULL, g_num_shortcut_state.timer_id);
            g_num_shortcut_state.timer_id = 0;
        }
        g_num_shortcut_state.timer_id = SetTimer(NULL, 0, NUMBER_SHORTCUT_DELAY_MS, NumberShortcutTimerProc);
        
        printf("[NUM-SHORTCUT] Waiting for second digit... (first=%d)\n", digit);
        return true;
    } else if (g_num_shortcut_state.state == 1) {
        int combined_number = (g_num_shortcut_state.first_digit * 10) + digit;
        
        if (g_num_shortcut_state.timer_id != 0) {
            KillTimer(NULL, g_num_shortcut_state.timer_id);
            g_num_shortcut_state.timer_id = 0;
        }
        
        printf("[NUM-SHORTCUT] Two-digit number: %d\n", combined_number);
        ExecuteNumberShortcut(combined_number, g_num_shortcut_state.close_popup);
        return true;
    }
    
    return false;
}

void ResetNumberShortcutState(void) {
    if (g_num_shortcut_state.timer_id != 0) {
        KillTimer(NULL, g_num_shortcut_state.timer_id);
        g_num_shortcut_state.timer_id = 0;
    }
    g_num_shortcut_state.state = 0;
    g_num_shortcut_state.first_digit = 0;
    g_num_shortcut_state.hListBox = NULL;
    g_num_shortcut_state.hEditControl = NULL;
}
