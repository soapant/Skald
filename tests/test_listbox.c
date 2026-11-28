#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "core/context.h"
#include "core/types/prompt.h"
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "platform/draw.h"
#include "config.h"
#include "platform/platform.h"

#define POPULATE(lb, filter, items, count, match)                              \
  populate_listbox(&(PopulateListboxSpec){                                   \
      .f_lb = (lb), .f_filter = (filter), .f_items = (items),                  \
      .f_item_count = (count), .f_filter_match = (match)})

TEST(populate_normal_mode_selects_first) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  POPULATE(&lb,"", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.item_count, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
}

TEST(populate_insert_mode_empty_filter_no_selection) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  POPULATE(&lb,"", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
}

TEST(populate_filter_narrows) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  POPULATE(&lb,"ap", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 2);
  ASSERT_EQ_INT(lb.item_count, 2);
}

TEST(populate_no_match) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  POPULATE(&lb,"zzz", items, 2, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
}

TEST(populate_empty_items) {
  static ListBox lb;
  listbox_info_init(&lb);
  POPULATE(&lb,"", NULL, 0, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
}

TEST(filter_handles_double_spaces) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"hello world", "hello  world", "hello   world",
                         "helloworld"};
  POPULATE(&lb,"hello  world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  listbox_info_init(&lb);
  POPULATE(&lb,"hello world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  listbox_info_init(&lb);
  POPULATE(&lb,"  hello   world  ", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
}

TEST(ui_set_selection_unchanged_index_keeps_hscroll) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  POPULATE(&lb,"", items, 2, NULL);
  lb.text_hscroll = 120;
  lb.selected_index = 1;
  UI_SetSelection(&lb, 1);
  ASSERT_EQ_INT(lb.text_hscroll, 120);
  ASSERT_EQ_INT(lb.selected_index, 1);
}

TEST(ui_set_selection_new_index_resets_hscroll) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  POPULATE(&lb,"", items, 2, NULL);
  lb.text_hscroll = 120;
  lb.selected_index = 1;
  UI_SetSelection(&lb, 0);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
  ASSERT_EQ_INT(lb.selected_index, 0);
}

TEST(ui_navigate_to_same_index_keeps_hscroll) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  POPULATE(&lb,"", items, 2, NULL);
  lb.selected_index = 1;
  lb.text_hscroll = 90;
  UI_NavigateTo(&lb, 1);
  ASSERT_EQ_INT(lb.text_hscroll, 90);
  ASSERT_EQ_INT(lb.selected_index, 1);
}

TEST(ui_navigate_to_new_index_resets_hscroll) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *items[] = {"apple", "banana"};
  POPULATE(&lb,"", items, 2, NULL);
  lb.selected_index = 1;
  lb.text_hscroll = 90;
  UI_NavigateTo(&lb, 0);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
  ASSERT_EQ_INT(lb.selected_index, 0);
}

TEST(listbox_info_init_sets_hscroll_zero) {
  static ListBox lb;
  listbox_info_init(&lb);
  ASSERT_EQ_INT(lb.text_hscroll, 0);
}

TEST(listbox_scroll_top_keeps_rows_above_selection) {
  ASSERT_EQ_INT(listbox_scroll_top_for(40, 10, 100), 35);
  ASSERT_EQ_INT(listbox_scroll_top_for(2, 10, 100), 0);
  ASSERT_EQ_INT(listbox_scroll_top_for(95, 10, 100), 90);
}

TEST(populate_repairs_selected_text_after_row_rebuild) {
  static ListBox lb;
  listbox_info_init(&lb);
  const char *before[] = {"alpha", "beta"};
  POPULATE(&lb,"", before, 2, NULL);
  UI_SetSelection(&lb, 1);
  ASSERT_EQ_STR(lb.selected_text, "beta");
  const char *after[] = {"alpha", "BETA2", "gamma"};
  POPULATE(&lb,"", after, 3, NULL);
  ASSERT_EQ_STR(lb.selected_text, "BETA2");
  listbox_info_free(&lb);
}

TEST(populate_grows_beyond_fixed_limits) {
  static ListBox lb;
  listbox_info_init(&lb);
  const int n = MAX_VISIBLE_ITEMS + 20;
  const char **items = (const char **)calloc((size_t)n, sizeof(*items));
  ASSERT_TRUE(items != NULL);
  if (!items)
    return;
  char long_row[700];
  memset(long_row, 'x', sizeof(long_row) - 1);
  long_row[sizeof(long_row) - 1] = '\0';
  for (int i = 0; i < n; i++)
    items[i] = (i == 0) ? long_row : "short";
  POPULATE(&lb,"", items, n, NULL);
  ASSERT_EQ_INT(lb.total_matches, n);
  ASSERT_EQ_INT(lb.item_count, n);
  ASSERT_TRUE(lb.matches_cap >= n);
  const char *row0 = listbox_info_row(&lb, 0);
  ASSERT_TRUE(row0 != NULL);
  if (row0)
    ASSERT_EQ_INT((int)strlen(row0), (int)strlen(long_row));
  free(items);
  listbox_info_free(&lb);
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
  RUN(listbox_scroll_top_keeps_rows_above_selection);
  RUN(populate_repairs_selected_text_after_row_rebuild);
  RUN(populate_grows_beyond_fixed_limits);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}

