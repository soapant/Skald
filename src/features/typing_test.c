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

void typing_test_render(PromptState *state) {
    int cy = state->height / 2 - 130;
    static int current_position = -1;
    static int pos_drawn = 0;
    char* text = "You are right, your current API";

    int c = tolower(fl_get_char_pressed());
    int key = fl_get_key_pressed();
    int current_text = tolower(text[current_position]);

    if (c == current_text) {
      fl_place_text(.x=current_position * 20, .y=cy,.align="Left",.text=text[current_position],.fontsize=30,.color=RGB(0,255,0));
      pos_drawn = current_position;
      current_position++;
    } else if (c != text[current_position] && c) {
      fl_place_text(.x=current_position * 20, .y=cy,.align="Left",.text=text[current_position],.fontsize=30,.color=RGB(0,0,255));
      pos_drawn = current_position;
      current_position++;
    }
    if (key == VK_BACK) {
      current_position--;
    }

    for (int i = 0; i < strlen(text);i++) {
      if (pos_drawn != i) {
        fl_place_text(.x=i * 20, .y=cy,.align="Left",.text=text[i],.fontsize=30);
      }
    }
    if (current_position == strlen(text) + 1) {
      exit(1);
    }
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
