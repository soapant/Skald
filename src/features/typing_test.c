#include "core/flapi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


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
static char typing_test_wpm[100] = {0};
static char* text = "";


char* typing_test_get_random_text(void) {
    static const char* words[] = {
        "the", "be", "of", "and", "a", "to", "in", "he", "have", "it",
        "that", "for", "they", "I", "with", "as", "not", "on", "she", "at",
        "by", "this", "we", "you", "do", "but", "from", "or", "which", "one",
        "would", "all", "will", "there", "say", "who", "make", "when", "can", "more",
        "if", "no", "man", "out", "other", "so",
        "what", "time", "up", "go", "about", "than", "into", "could", "state", "only",
        "new", "year", "some", "take", "come", "these",
        "know", "see", "use", "get", "like", "then",
        "first", "any", "work", "now", "may", "such",
        "give", "over", "think", "most", "even", "back",
        "after", "also", "because", "day", "good", "through",
        "long", "where", "much", "before", "great", "should",
        "between", "own", "under", "last", "never", "while",
        "might", "world", "still", "see", "three", "state",
        "another", "part", "against", "place", "case", "week",
        "company", "system", "program", "question", "number", "group",
        "problem", "fact", "become", "interest", "information", "example",
        "family", "student", "study", "school", "country", "city",
        "problem", "service", "member", "community", "development", "research",
        "technology", "education", "government", "business", "health", "environment",
    };
    const int num_words = sizeof(words) / sizeof(words[0]);
    const int target_count = 30;

    int indices[target_count];
    size_t total_len = 1;
    for (int i = 0; i < target_count; i++) {
        indices[i] = rand() % num_words;
        total_len += strlen(words[indices[i]]);
        if (i < target_count - 1) total_len += 1;
    }

    char* result = malloc(total_len);
    if (!result) return NULL;

    char* p = result;
    for (int i = 0; i < target_count; i++) {
        const char* w = words[indices[i]];
        size_t len = strlen(w);
        memcpy(p, w, len);
        p += len;
        if (i < target_count - 1) *p++ = ' ';
    }
    *p = '\0';

    return result;
}

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
    text = typing_test_get_random_text();
}


bool typing_test_enter(const char* selection, ListBoxInfo* lb) {
    typing_test_reset();
    text = typing_test_get_random_text();
    return false;
}


void typing_test_render(PromptState *state) {
    int cy = state->height / 2 - 130;
    int text_len = strlen(text);
    int fontsize = 30;
    int line_height = fontsize + 12;

    int c = fl_get_char_pressed();
    int key = fl_get_key_pressed();
    if (typing_test_position < text_len) {
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
                      .align = "Left", .text = single, .color = col);
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
        fl_draw_text(.y=500,.text=typing_test_wpm,.fontsize=fontsize);
    }
    fl_draw_text(.x = 250,.y=600,.text="Press Enter to restart",.fontsize=fontsize - 8,.color=RGB(180,180,180));
    fl_draw_text(.x = -250,.y=600,.text="Press Escape to exit",.fontsize=fontsize - 8,.color=RGB(180,180,180));
    fl_draw_text(.y=0,.text="Typing Speed Test",.fontsize=fontsize - 8,.color=RGB(210,210,210));
    if (draw_cursor) {
      fl_draw_rect(.x = positions[typing_test_position].x,
                   .y = positions[typing_test_position].y + 6,
                   .w = 1, .h = fontsize);
    }
}

void TriggerTypingTest(void) {
    PROMPT(.enter = typing_test_enter,
           .render = typing_test_render,
           .on_close = typing_test_reset);
}

FLINT_FEATURE_INIT {
    srand((unsigned)time(NULL));
    text = typing_test_get_random_text();
    fl_register_hotkey("Alt+B", TriggerTypingTest);
    fl_register_feature("Typing Test", TriggerTypingTest);
}
