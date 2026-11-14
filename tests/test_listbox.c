#include "test.h"
#include "core/context.h"
#include "core/types/prompt.h"
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "platform/draw.h"
#include "config.h"
#include "platform/platform.h"

TEST(populate_normal_mode_selects_first) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  populate_listbox(&lb, "", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.item_count, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
}

TEST(populate_insert_mode_empty_filter_no_selection) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  populate_listbox(&lb, "", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
}

TEST(populate_filter_narrows) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  populate_listbox(&lb, "ap", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 2);
  ASSERT_EQ_INT(lb.item_count, 2);
}

TEST(populate_no_match) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  populate_listbox(&lb, "zzz", items, 2, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
}

TEST(populate_empty_items) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  populate_listbox(&lb, "", NULL, 0, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
}

TEST(filter_handles_double_spaces) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"hello world", "hello  world", "hello   world",
                         "helloworld"};
  populate_listbox(&lb, "hello  world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  listbox_info_init(&lb);
  populate_listbox(&lb, "hello world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  listbox_info_init(&lb);
  populate_listbox(&lb, "  hello   world  ", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
}

TEST(ui_set_selection_unchanged_index_keeps_hscroll) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  populate_listbox(&lb, "", items, 2, NULL);
  lb.text_hscroll = 120;
  lb.selected_index = 1;
  UI_SetSelection(&lb, 1);
  ASSERT_EQ_INT(lb.text_hscroll, 120);
  ASSERT_EQ_INT(lb.selected_index, 1);
}

TEST(ui_set_selection_new_index_resets_hscroll) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  populate_listbox(&lb, "", items, 2, NULL);
  lb.text_hscroll = 120;
  lb.selected_index = 1;
  UI_SetSelection(&lb, 0);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
  ASSERT_EQ_INT(lb.selected_index, 0);
}

TEST(ui_navigate_to_same_index_keeps_hscroll) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  populate_listbox(&lb, "", items, 2, NULL);
  lb.selected_index = 1;
  lb.text_hscroll = 90;
  UI_NavigateTo(&lb, 1);
  ASSERT_EQ_INT(lb.text_hscroll, 90);
  ASSERT_EQ_INT(lb.selected_index, 1);
}

TEST(ui_navigate_to_new_index_resets_hscroll) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  populate_listbox(&lb, "", items, 2, NULL);
  lb.selected_index = 1;
  lb.text_hscroll = 90;
  UI_NavigateTo(&lb, 0);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
  ASSERT_EQ_INT(lb.selected_index, 0);
}

TEST(listbox_info_init_sets_hscroll_zero) {
  static ListBoxInfo lb;
  listbox_info_init(&lb);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
}
int main(void) {
  RUN(populate_normal_mode_selects_first);
  RUN(populate_insert_mode_empty_filter_no_selection);
  RUN(populate_filter_narrows);
  RUN(populate_no_match);
  RUN(populate_empty_items);
  RUN(filter_handles_double_spaces);
  RUN(ui_set_selection_unchanged_index_keeps_hscroll);
  RUN(ui_set_selection_new_index_resets_hscroll);
  RUN(ui_navigate_to_same_index_keeps_hscroll);
  RUN(ui_navigate_to_new_index_resets_hscroll);
  RUN(listbox_info_init_sets_hscroll_zero);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
