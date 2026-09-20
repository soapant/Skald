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
    char* text = "thats kind of bullshit imagine a game like that imagine you are on your pc then a game installs itself and runs itself and you cant leave then you have to play the game for years just so you can eventually get out of it?  why the fuck is there a space before the text shit did it again mb me";
    int text_len = strlen(text);

    static int current_position = 0;
    static int char_states[1000] = {0};

    if (current_position < text_len) {
        int c = fl_get_char_pressed();
        int key = fl_get_key_pressed();
        int current_text = text[current_position];

        if (key == FL_KEY_BACKSPACE) {
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
            fl_place_text(.x=i * 20, .y=cy, .align="Left", .text=text[i], .fontsize=30, .color=RGB(255,0,0));
        } else {
            fl_place_text(.x=i * 20, .y=cy, .align="Left", .text=text[i], .fontsize=30);
        }
    }

    if (current_position == text_len) {
        exit(1); 
    }

    fl_draw_rect(.x=current_position * 20, .y=cy + 10, .w=1, .h=30);
}

void TriggerTypingTest(void) {
    PROMPT(.refresh = typing_test_refresh,
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
