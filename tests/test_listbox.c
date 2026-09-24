#include "test.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/types/prompt.h"
#include "helpers/helpers_ui.h"

TEST(populate_normal_mode_selects_first) {
    SetMode(MODE_NORMAL);
    ListBoxInfo lb;
    ListBoxInfo_Init(&lb);
    const char* items[] = {"apple", "banana", "apricot"};
    PopulateListBox(&lb, (WindowHandle){0}, "", items, 3, NULL);
    ASSERT_EQ_INT(lb.total_matches, 3);
    ASSERT_EQ_INT(lb.populated_count, 3);
    ASSERT_EQ_INT(lb.selected_index, 0);
    ASSERT_TRUE(UI_IsListBoxSelected(&lb));
}

TEST(populate_insert_mode_empty_filter_no_selection) {
    SetMode(MODE_INSERT);
    ListBoxInfo lb;
    ListBoxInfo_Init(&lb);
    const char* items[] = {"apple", "banana", "apricot"};
    PopulateListBox(&lb, (WindowHandle){0}, "", items, 3, NULL);
    ASSERT_EQ_INT(lb.total_matches, 3);
    ASSERT_EQ_INT(lb.selected_index, -1);
    ASSERT_FALSE(UI_IsListBoxSelected(&lb));
}

TEST(populate_filter_narrows) {
    ListBoxInfo lb;
    ListBoxInfo_Init(&lb);
    const char* items[] = {"apple", "banana", "apricot"};
    PopulateListBox(&lb, (WindowHandle){0}, "ap", items, 3, NULL);
    ASSERT_EQ_INT(lb.total_matches, 2);
    ASSERT_EQ_INT(lb.populated_count, 2);
}

TEST(populate_no_match) {
    ListBoxInfo lb;
    ListBoxInfo_Init(&lb);
    const char* items[] = {"apple", "banana"};
    PopulateListBox(&lb, (WindowHandle){0}, "zzz", items, 2, NULL);
    ASSERT_EQ_INT(lb.total_matches, 0);
    ASSERT_EQ_INT(lb.populated_count, 0);
    ASSERT_FALSE(UI_IsListBoxSelected(&lb));
}

TEST(populate_empty_items) {
    ListBoxInfo lb;
    ListBoxInfo_Init(&lb);
    PopulateListBox(&lb, (WindowHandle){0}, "", NULL, 0, NULL);
    ASSERT_EQ_INT(lb.total_matches, 0);
    ASSERT_EQ_INT(lb.populated_count, 0);
}

int main(void) {
    RUN(populate_normal_mode_selects_first);
    RUN(populate_insert_mode_empty_filter_no_selection);
    RUN(populate_filter_narrows);
    RUN(populate_no_match);
    RUN(populate_empty_items);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
