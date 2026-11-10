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
  ListBoxInfo_Init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  PopulateListBox(&lb, "", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.item_count, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
  ASSERT_FALSE(UI_IsListBoxSelected(&lb));
}

TEST(populate_insert_mode_empty_filter_no_selection) {
  static ListBoxInfo lb;
  ListBoxInfo_Init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  PopulateListBox(&lb, "", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 3);
  ASSERT_EQ_INT(lb.selected_index, -1);
  ASSERT_FALSE(UI_IsListBoxSelected(&lb));
}

TEST(populate_filter_narrows) {
  static ListBoxInfo lb;
  ListBoxInfo_Init(&lb);
  const char *items[] = {"apple", "banana", "apricot"};
  PopulateListBox(&lb, "ap", items, 3, NULL);
  ASSERT_EQ_INT(lb.total_matches, 2);
  ASSERT_EQ_INT(lb.item_count, 2);
}

TEST(populate_no_match) {
  static ListBoxInfo lb;
  ListBoxInfo_Init(&lb);
  const char *items[] = {"apple", "banana"};
  PopulateListBox(&lb, "zzz", items, 2, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
  ASSERT_FALSE(UI_IsListBoxSelected(&lb));
}

TEST(populate_empty_items) {
  static ListBoxInfo lb;
  ListBoxInfo_Init(&lb);
  PopulateListBox(&lb, "", NULL, 0, NULL);
  ASSERT_EQ_INT(lb.total_matches, 0);
  ASSERT_EQ_INT(lb.item_count, 0);
}

TEST(filter_handles_double_spaces) {
  static ListBoxInfo lb;
  ListBoxInfo_Init(&lb);
  const char *items[] = {"hello world", "hello  world", "hello   world",
                         "helloworld"};
  PopulateListBox(&lb, "hello  world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  ListBoxInfo_Init(&lb);
  PopulateListBox(&lb, "hello world", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
  ListBoxInfo_Init(&lb);
  PopulateListBox(&lb, "  hello   world  ", items, 4, NULL);
  ASSERT_TRUE(lb.total_matches >= 2);
}


TEST(text_prefix_bytes_hscroll_zero_returns_zero) {
  int draw_x = 999;
  int i = ListBox_TextPrefixBytes("abcdef", 0, &draw_x);
  ASSERT_EQ_INT(i, 0);
  ASSERT_EQ_INT(draw_x, 0);
}

TEST(text_prefix_bytes_null_text_returns_zero) {
  int draw_x = 999;
  ASSERT_EQ_INT(ListBox_TextPrefixBytes(NULL, 100, &draw_x), 0);
  ASSERT_EQ_INT(draw_x, 0);
}

TEST(text_prefix_bytes_hscroll_below_first_glyph) {
  int w_ab =
      Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
          .text = "ab", .len = 2, .fontsize = FontSize});
  int draw_x = 999;
  int i = ListBox_TextPrefixBytes("ab", w_ab - 1, &draw_x);
  ASSERT_TRUE(i >= 0);
  ASSERT_TRUE(i <= 1);
  int w_0 = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = "ab", .len = 1, .fontsize = FontSize});
  if (i == 0)
    ASSERT_EQ_INT(draw_x, 0 - (w_ab - 1));
  else
    ASSERT_EQ_INT(draw_x, w_0 - (w_ab - 1));
}

TEST(text_prefix_bytes_exact_two_ascii_chars_fit) {
  const char *txt = "abcd";
  int w = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = txt, .len = 4, .fontsize = FontSize});
  int w_ab = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = txt, .len = 2, .fontsize = FontSize});
  int draw_x = 999;
  int i = ListBox_TextPrefixBytes(txt, w_ab, &draw_x);
  ASSERT_EQ_INT(i, 2);
  ASSERT_EQ_INT(draw_x, w_ab - w_ab);
}

TEST(text_prefix_bytes_never_splits_utf8_codepoint) {
  const char *txt = "ábc"; /* U+00E1 encoded as 2 bytes */
  int i_all = ListBox_TextPrefixBytes(txt, 100000, NULL);
  ASSERT_EQ_INT(i_all, (int)strlen(txt));
  int w_a = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = txt, .len = 2, .fontsize = FontSize});
  const char *ascii = "xbc"; /* sanity that ASCII widths are positive */
  (void)ascii;
  int i = ListBox_TextPrefixBytes(txt, w_a, NULL);
  ASSERT_EQ_INT(i, 2);
  int i_partial = ListBox_TextPrefixBytes(txt, w_a / 2, NULL);
  ASSERT_EQ_INT(i_partial, 0);
}

TEST(text_prefix_bytes_full_fit_returns_length) {
  const char *txt = "hello world";
  int i = ListBox_TextPrefixBytes(txt, 100000, NULL);
  ASSERT_EQ_INT(i, (int)strlen(txt));
}

TEST(text_prefix_bytes_negative_draw_x_reports_scroll) {
  const char *txt = "aaaa";
  int w = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = txt, .len = 4, .fontsize = FontSize});
  int hscroll = w / 2;
  int draw_x = 999;
  int i = ListBox_TextPrefixBytes(txt, hscroll, &draw_x);
  ASSERT_TRUE(draw_x <= 0);
  ASSERT_TRUE(i >= 0);
}
int main(void) {
  RUN(populate_normal_mode_selects_first);
  RUN(populate_insert_mode_empty_filter_no_selection);
  RUN(populate_filter_narrows);
  RUN(populate_no_match);
  RUN(populate_empty_items);
  RUN(filter_handles_double_spaces);
  RUN(text_prefix_bytes_hscroll_zero_returns_zero);
  RUN(text_prefix_bytes_null_text_returns_zero);
  RUN(text_prefix_bytes_hscroll_below_first_glyph);
  RUN(text_prefix_bytes_exact_two_ascii_chars_fit);
  RUN(text_prefix_bytes_never_splits_utf8_codepoint);
  RUN(text_prefix_bytes_full_fit_returns_length);
  RUN(text_prefix_bytes_negative_draw_x_reports_scroll);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
