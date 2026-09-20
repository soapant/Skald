#include "core/flapi.h"

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

static int typing_test_position = 0;
static int typing_test_states[1000] = {0};
static FlTimer current_timer;
static bool timer_running = false;
static int current_index = 0;
static double last_duration = 0.0;
static FlTimer session_timer;
static bool session_running = false;
static double total_typed = 0;
static double total_correct = 0;
static bool draw_cursor = true;
static char typing_test_wpm[200] = {0};

void typing_test_reset(void) {
    typing_test_position = 0;
    memset(typing_test_states, 0, sizeof(typing_test_states));
    timer_running = false;
    session_running = false;
    current_index = 0;
    last_duration = 0.0;
    total_typed = 0;
    total_correct = 0;
    draw_cursor = true;
    typing_test_wpm[0] = '\0';
}


void typing_test_render(PromptState *state) {
    int cy = state->height / 2 - 130;
    char* text = "thats kind of bullshit imagine a game like that imagine you are on your pc then a game installs itself";
    int text_len = strlen(text);
    int fontsize = 30;
    int line_height = fl_font_line_height(fontsize);

    if (typing_test_position < text_len) {
        int c = fl_get_char_pressed();
        int key = fl_get_key_pressed();
        int current_text = text[typing_test_position];

        if (key == FL_KEY_BACKSPACE) {
            if (typing_test_position > 0) {
                typing_test_position--;
                typing_test_states[typing_test_position] = CHAR_UNTYPED;
            }
        }
        else if (c != 0) {
            if (!session_running) {
                session_timer = fl_start_timer();
                session_running = true;
            }

            if (!timer_running) {
                current_timer = fl_start_timer();
                timer_running = true;
            } else {
                current_timer = fl_start_timer();
            }

            total_typed++;
            if (c == current_text) {
                typing_test_states[typing_test_position] = CHAR_CORRECT;
                total_correct++;
            } else {
                typing_test_states[typing_test_position] = CHAR_WRONG;
            }
            typing_test_position++;
        }
    }

    FlCharPos positions[1000];
    fl_measure_text_wrapped(.x=0, .y=cy, .max_width=state->width, .line_height=line_height, .fontsize=fontsize, .text=text, .out_positions=positions);

    for (int i = 0; i < text_len; i++) {
        unsigned long col;
        if (typing_test_states[i] == CHAR_CORRECT) {
            col = RGB(0, 255, 0);
        } else if (typing_test_states[i] == CHAR_WRONG) {
            col = RGB(255, 0, 0);
        } else {
            col = RGB(255, 255, 255);
        }
        char single[2] = { text[i], '\0' };
        fl_draw_text(.x = positions[i].x, .y = positions[i].y, .fontsize = fontsize,
                      .align = "LEFT", .text = single, .color = col);
    }

    if (typing_test_position == text_len) {
        draw_cursor = false;
        if (timer_running) {
            timer_running = false;
        }
        if (session_running) {
            session_running = false;

            double elapsed_ms = fl_end_timer(session_timer);
            double elapsed = elapsed_ms / 1000.0;
            double minutes = elapsed / 60.0;
            double accuracy = total_correct / total_typed;
            double gross_wpm = (total_typed / 5.0) / minutes;
            double wpm = gross_wpm * accuracy;

            snprintf(typing_test_wpm,sizeof(typing_test_wpm),"WPM: %.2f",wpm);
        }
        fl_draw_text(.y=500,.text=typing_test_wpm);
    }
    if (draw_cursor) {
      fl_draw_rect(.x = positions[typing_test_position].x,
                   .y = positions[typing_test_position].y + (line_height - fontsize) / 2,
                   .w = 1, .h = fontsize);
    }
}

void TriggerTypingTest(void) {
    PROMPT(.enter = typing_test_enter,
           .ctrl_enter = typing_test_ctrl_enter,
           .copy = typing_test_copy,
           .tab = typing_test_tab,
           .render = typing_test_render,
           .on_close = typing_test_reset);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+B", TriggerTypingTest);
    fl_register_feature("Typing Test", TriggerTypingTest);
}
