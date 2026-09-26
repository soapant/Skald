#include "test.h"
#include "config.h"
#include "platform/platform.h"
#include <stdbool.h>

void Platform_GetTextLayoutMetrics(int* line_h, int* row_h, int* list_top);

static int g_saved_font_size;

#define EDIT_TEXT_TOP 20

static void metrics(int* line_h, int* row_h, int* list_top) {
    Platform_GetTextLayoutMetrics(line_h, row_h, list_top);
}

TEST(metrics_are_available_for_the_current_font) {
    int lh = 0, rh = 0, lt = 0;
    metrics(&lh, &rh, &lt);
    ASSERT_TRUE(lh > 0);
    ASSERT_TRUE(rh > 0);
    ASSERT_TRUE(lt > 0);
}

TEST(line_height_exceeds_the_em_size_at_every_size) {
    g_saved_font_size = FontSize;
    for (int size = 8; size <= 48; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        if (lh <= size) {
            printf("  (font size %d: line height %d is not larger than the em size)\n", size, lh);
            flint_test_fails++;
        }
    }
    FontSize = g_saved_font_size;
}

TEST(list_never_starts_above_the_bottom_of_the_edit_text) {
    g_saved_font_size = FontSize;
    for (int size = 8; size <= 48; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        int text_bottom = EDIT_TEXT_TOP + lh;
        if (lt <= text_bottom) {
            printf("  (font size %d: list top %d collides with edit text bottom %d)\n",
                   size, lt, text_bottom);
            flint_test_fails++;
        }
    }
    FontSize = g_saved_font_size;
}

TEST(row_height_is_taller_than_the_line_box) {
    g_saved_font_size = FontSize;
    for (int size = 8; size <= 48; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        if (rh <= lh) {
            printf("  (font size %d: row height %d does not exceed line height %d)\n",
                   size, rh, lh);
            flint_test_fails++;
        }
    }
    FontSize = g_saved_font_size;
}

TEST(line_height_grows_with_font_size) {
    g_saved_font_size = FontSize;
    int prev = 0;
    for (int size = 8; size <= 48; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        if (lh < prev) {
            printf("  (font size %d: line height %d shrank from %d)\n", size, lh, prev);
            flint_test_fails++;
        }
        prev = lh;
    }
    FontSize = g_saved_font_size;
}

TEST(list_top_grows_with_font_size) {
    g_saved_font_size = FontSize;
    int prev = -1;
    for (int size = 8; size <= 48; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        if (prev >= 0 && lt <= prev) {
            printf("  (font size %d: list top %d did not grow past %d)\n", size, lt, prev);
            flint_test_fails++;
        }
        prev = lt;
    }
    FontSize = g_saved_font_size;
}

TEST(metrics_are_stable_across_repeated_calls) {
    g_saved_font_size = FontSize;
    FontSize = 20;
    int lh0 = 0, rh0 = 0, lt0 = 0;
    metrics(&lh0, &rh0, &lt0);
    for (int i = 0; i < 3; i++) {
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
        ASSERT_EQ_INT(lh, lh0);
        ASSERT_EQ_INT(rh, rh0);
        ASSERT_EQ_INT(lt, lt0);
    }
    FontSize = g_saved_font_size;
}

TEST(metrics_survive_font_size_churn) {
    g_saved_font_size = FontSize;
    FontSize = 20;
    int lh0 = 0, rh0 = 0, lt0 = 0;
    metrics(&lh0, &rh0, &lt0);
    for (int size = 10; size <= 40; size++) {
        FontSize = size;
        int lh = 0, rh = 0, lt = 0;
        metrics(&lh, &rh, &lt);
    }
    FontSize = 20;
    int lh1 = 0, rh1 = 0, lt1 = 0;
    metrics(&lh1, &rh1, &lt1);
    ASSERT_EQ_INT(lh1, lh0);
    ASSERT_EQ_INT(rh1, rh0);
    ASSERT_EQ_INT(lt1, lt0);
    FontSize = g_saved_font_size;
}

int main(void) {
    g_saved_font_size = FontSize;
    RUN(metrics_are_available_for_the_current_font);
    RUN(line_height_exceeds_the_em_size_at_every_size);
    RUN(list_never_starts_above_the_bottom_of_the_edit_text);
    RUN(row_height_is_taller_than_the_line_box);
    RUN(line_height_grows_with_font_size);
    RUN(list_top_grows_with_font_size);
    RUN(metrics_are_stable_across_repeated_calls);
    RUN(metrics_survive_font_size_churn);
    FontSize = g_saved_font_size;
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
