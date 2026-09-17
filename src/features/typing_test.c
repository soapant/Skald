#include "core/flapi.h"

void typing_test_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
}

bool typing_test_enter(const char* selection, ListBoxInfo* lb) {
    return true;
}

bool typing_test_ctrl_enter(const char* selection, ListBoxInfo* lb) {
    return false;
}

void typing_test_copy(const char* selection, int true_index, int type) {
}

void typing_test_tab(ListBoxInfo* lb) {
}


#define CHAR_UNTYPED 0
#define CHAR_CORRECT 1
#define CHAR_WRONG   2

void typing_test_render(PromptState *state) {
    int cy = state->height / 2 - 130;
    char* text = "You are right, your current API";
    int text_len = strlen(text);

    static int current_position = 0;
    static int char_states[100] = {0};

    if (current_position < text_len) {
        int c = tolower(fl_get_char_pressed());
        int key = fl_get_key_pressed();
        int current_text = tolower(text[current_position]);

        if (key == VK_BACK) {
            if (current_position > 0) {
                current_position--;
                char_states[current_position] = CHAR_UNTYPED;
            }
        } 
        else if (c != 0) { 
            if (c == current_text) {
                char_states[current_position] = CHAR_CORRECT;
                current_position++;
            } else {
                char_states[current_position] = CHAR_WRONG;
                current_position++;
            }
        }
    }

    for (int i = 0; i < text_len; i++) {
        if (char_states[i] == CHAR_CORRECT) {
            fl_place_text(.x=i * 20, .y=cy, .align="Left", .text=text[i], .fontsize=30, .color=RGB(0,255,0));
        } else if (char_states[i] == CHAR_WRONG)  {
            fl_place_text(.x=i * 20, .y=cy, .align="Left", .text=text[i], .fontsize=30, .color=RGB(0,0,255));
        } else {
            fl_place_text(.x=i * 20, .y=cy, .align="Left", .text=text[i], .fontsize=30);
        }
    }

    if (current_position == text_len) {
        exit(1); 
    }

    fl_draw_rect(current_position * 20, cy + 10, 1, 30, COLOR_TEXT);
}

void TriggerTypingTest(void) {
    PROMPT(.title = "Typing Test",
           .refresh = typing_test_refresh,
           .enter = typing_test_enter,
           .ctrl_enter = typing_test_ctrl_enter,
           .copy = typing_test_copy,
           .tab = typing_test_tab,
           .render = typing_test_render);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+B", TriggerTypingTest);
    fl_register_feature("Typing Test", TriggerTypingTest);
}
