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
    int cx = 0;
    int cy = state->height / 2 - 30;
    char* text = "You are right, your current API only lets you draw the whole string at once, which is a problem for a typing test. The good news is that there's a standard way to handle this without needing a major API rewrite";
    for (int i = 0; i < strlen(text);i++) {
      fl_place_text(.x=cx + (i * 20), .y=cy,.align="Right",.text=text[i],.fontsize=30);
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
