#include "core/flapi.h"
#include "config.h"
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


static int typing_test_word_count = 30;

char* typing_test_get_random_text(int word_count) {
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
        "computer", "internet", "phone", "tablet", "tv", "camera",
        "car", "bike", "boat", "plane", "train", "bus",
    };
    const int num_words = sizeof(words) / sizeof(words[0]);
    int target_count = word_count > 0 ? word_count : 30;
    if (target_count > num_words) target_count = num_words;

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
    text = typing_test_get_random_text(typing_test_word_count);
}


bool typing_test_enter(const char* selection, ListBoxInfo* lb) {
    typing_test_reset();
    text = typing_test_get_random_text(typing_test_word_count);
    return false;
}


static int typing_test_measure_lines(FlCharPos* pos, int count, const char* txt,
                                     int y, int max_width, int line_height, int fs) {
    fl_measure_text_wrapped(.x=0, .y=y, .max_width=max_width,
                            .line_height=line_height, .fontsize=fs,
                            .text=txt, .out_positions=pos);
    int lines = 0;
    int last_y = -1000000;
    for (int i = 0; i < count; i++) {
        if (pos[i].y != last_y) { lines++; last_y = pos[i].y; }
    }
    return lines;
}

void typing_test_render(PromptState *state) {
    int base = FontSize;
    if (base < 8) base = 8;
    int W = state->width > 0 ? state->width : 900;
    int H = state->height > 0 ? state->height : 640;

    int body = base * 3 / 2;
    if (body > H / 3) body = H / 3;
    if (body < 12) body = 12;
    int line_height = body + base / 2;
    int margin = base * 2;

    int title_y = base;
    int text_y = H / 2 - (base * 4);
    int hints_y = H - base * 3;
    int status_y = hints_y - base * 2;
    int limit_y = status_y - base;
    if (limit_y <= text_y) limit_y = text_y + body;
    int avail = limit_y - text_y;

    int text_len = text ? (int)strlen(text) : 0;
    int c = fl_get_char_pressed();
    int key = fl_get_key_pressed();
    if (text && typing_test_position < text_len) {
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

    {
        int lh = body + base / 2;
        if (lh < 1) lh = 1;
        int per_line = (W - margin * 2) / (body / 2 + 1);
        if (per_line < 1) per_line = 1;
        int max_lines = avail / lh;
        if (max_lines < 1) max_lines = 1;
        int words = (max_lines * per_line) / 6;
        if (words > 30) words = 30;
        if (words < 6) words = 6;
        if (words != typing_test_word_count) {
            typing_test_word_count = words;
            free(text);
            text = typing_test_get_random_text(words);
            text_len = text ? (int)strlen(text) : 0;
            typing_test_position = 0;
            draw_cursor = true;
            memset(typing_test_states, 0, sizeof(typing_test_states));
        }
    }

    FlCharPos* positions = NULL;
    if (text_len > 0) {
        positions = (FlCharPos*)malloc(sizeof(FlCharPos) * (size_t)(text_len + 1));
    }

    if (positions) {
        memset(positions, 0, sizeof(FlCharPos) * (size_t)(text_len + 1));
        typing_test_measure_lines(positions, text_len, text, text_y,
                                 W - margin * 2, line_height, body);

        if (draw_cursor && typing_test_position < text_len) {
            fl_draw_rect(.x = positions[typing_test_position].x + margin + 2,
                         .y = positions[typing_test_position].y + base / 4,
                         .w = body / 2, .h = body,.transparent=true);
        }

        for (int i = 0; i < text_len; i++) {
            unsigned long col;
            if (typing_test_states[i] == CHAR_CORRECT) {
                col = RGB(0, 255, 0);
            } else if (typing_test_states[i] == CHAR_WRONG) {
                col = RGB(255, 0, 0);
            } else {
                col = NULL;
            }
            char single[2] = { text[i], '\0' };
            fl_draw_text(.x = positions[i].x + margin, .y = positions[i].y, .fontsize = body,
                          .align = "Left", .text = single, .color = col);
        }
    }

    if (typing_test_position == text_len && text_len > 0) {
        draw_cursor = false;
        if (timer_running) {
            timer_running = false;
        }
        if (session_running) {
            session_running = false;

            double elapsed_ms = fl_end_timer(session_timer);
            double elapsed = elapsed_ms / 1000.0;
            double minutes = elapsed / 60.0;
            double accuracy = total_typed > 0 ? total_correct / total_typed : 0.0;
            double gross_wpm = (total_typed / 5.0) / (minutes > 0.0001 ? minutes : 0.0001);
            double wpm = gross_wpm * accuracy;

            snprintf(typing_test_wpm,sizeof(typing_test_wpm),"WPM: %.2f",wpm);
        }
        if (typing_test_wpm[0]) {
            fl_draw_text(.x=W,.y=status_y,.text=typing_test_wpm,.fontsize=base,.align="Center");
        }
    }
    fl_draw_text(.x = margin,.y=hints_y,.text="Press Enter to restart",
                 .fontsize=base,.color=RGB(180,180,180),.align="Left");
    fl_draw_text(.x = W - margin,.y=hints_y,.text="Press Escape to exit",
                 .fontsize=base,.color=RGB(180,180,180),.align="Right");
    fl_draw_text(.x = W,.y=title_y,.text="Typing Speed Test",
                 .fontsize=base,.color=RGB(210,210,210),.align="Center");

    free(positions);
}

void TriggerTypingTest(void) {
    PROMPT(.enter = typing_test_enter,
           .render = typing_test_render,
           .on_close = typing_test_reset);
}

FLINT_FEATURE_INIT {
    srand((unsigned)time(NULL));
    text = typing_test_get_random_text(typing_test_word_count);
    fl_register_hotkey("Alt+B", TriggerTypingTest);
    fl_register_feature("Typing Test", TriggerTypingTest);
}
