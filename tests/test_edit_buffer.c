#include <stdio.h>
#include <string.h>

#include "test.h"

#include "helpers/helpers_edit_buffer.h"
#include "config_values.h"

TEST(backspace_removes_before_cursor) {
  char buf[64] = "hello";
  size_t cur = 5;
  EditBufferBackspace(buf, &cur);
  ASSERT_EQ_STR(buf, "hell");
  ASSERT_EQ_INT(cur, 4);

  char b3[8] = "abc";
  size_t mid = 1;
  EditBufferBackspace(b3, &mid);
  ASSERT_EQ_STR(b3, "bc");
  ASSERT_EQ_INT(mid, 0);
}

TEST(backspace_at_zero_is_noop) {
  char buf[8] = "ab";
  size_t cur = 0;
  EditBufferBackspace(buf, &cur);
  ASSERT_EQ_STR(buf, "ab");
  ASSERT_EQ_INT(cur, 0);
}

TEST(delete_char_middle_and_ends) {
  char b[16] = "bca";
  size_t cur = 0;
  ASSERT_TRUE(EditBufferDeleteChar(b, &cur));
  ASSERT_EQ_STR(b, "ca");
  ASSERT_EQ_INT(cur, 0);

  char b2[16] = "bca";
  size_t mid = 1;
  ASSERT_TRUE(EditBufferDeleteChar(b2, &mid));
  ASSERT_EQ_STR(b2, "ba");
  ASSERT_EQ_INT(mid, 1);

  char b3[16] = "bca";
  size_t end = strlen(b3);
  ASSERT_FALSE(EditBufferDeleteChar(b3, &end));
  ASSERT_EQ_STR(b3, "bca");
}

TEST(cursor_movement_bounds) {
  size_t cur = 0;
  ASSERT_TRUE(CursorForward(&cur, 3));
  ASSERT_EQ_INT(cur, 1);
  cur = 3;
  ASSERT_FALSE(CursorForward(&cur, 3));
  ASSERT_EQ_INT(cur, 3);
}

TEST(pair_for_round_pairs) {
  ASSERT_EQ_INT(EditBufferPairFor('{'), '}');
  ASSERT_EQ_INT(EditBufferPairFor('('), ')');
  ASSERT_EQ_INT(EditBufferPairFor('['), ']');
  ASSERT_EQ_INT(EditBufferPairFor('"'), '"');
  ASSERT_EQ_INT(EditBufferPairFor('\''), '\'');
  ASSERT_EQ_INT(EditBufferPairFor('x'), 0);
}

TEST(insert_into_middle) {
  EnableAutoPairing = false;
  char buf[16] = "ac";
  size_t cur = 1;
  size_t total = EditBufferInsertText(buf, sizeof(buf), &cur, "b", 1);
  ASSERT_EQ_INT(total, 1);
  ASSERT_EQ_STR(buf, "abc");
  ASSERT_EQ_INT(cur, 2);
}

TEST(insert_respects_capacity) {
  EnableAutoPairing = false;
  char buf[4] = "abc";
  size_t cur = 3;
  size_t total = EditBufferInsertText(buf, sizeof(buf), &cur, "yy", 2);
  ASSERT_EQ_INT(total, 0);
  ASSERT_EQ_STR(buf, "abc");

  char small[4] = "abc";
  size_t end = 3;
  ASSERT_EQ_INT(EditBufferInsertText(small, sizeof(small), &end, "y", 1), 0);
  ASSERT_EQ_STR(small, "abc");
}

TEST(insert_autopair_opener) {
  EnableAutoPairing = true;

  char buf[16] = "";
  size_t cur = 0;
  size_t total = EditBufferInsertText(buf, sizeof(buf), &cur, "{", 1);
  ASSERT_EQ_INT(total, 2);
  ASSERT_EQ_STR(buf, "{}");
  ASSERT_EQ_INT(cur, 1);

  char b4[16] = "";
  size_t start = 0;
  ASSERT_EQ_INT(EditBufferInsertText(b4, sizeof(b4), &start, "\"", 1), 2);
  ASSERT_EQ_STR(b4, "\"\"");
  ASSERT_EQ_INT(start, 1);

  EnableAutoPairing = false;
}

TEST(insert_quote_after_letter_is_not_paired) {
  EnableAutoPairing = true;

  char buf[16] = "ab";
  size_t cur = strlen(buf);
  size_t total = EditBufferInsertText(buf, sizeof(buf), &cur, "\"", 1);
  ASSERT_EQ_INT(total, 1);
  ASSERT_EQ_STR(buf, "ab\"");
  ASSERT_EQ_INT(cur, 3);

  EnableAutoPairing = false;
}

TEST(insert_skips_over_closer) {
  EnableAutoPairing = true;

  char buf[16] = "()";
  size_t cur = 1;
  size_t total = EditBufferInsertText(buf, sizeof(buf), &cur, ")", 1);
  ASSERT_EQ_INT(total, 0);
  ASSERT_EQ_STR(buf, "()");
  ASSERT_EQ_INT(cur, 2);

  EnableAutoPairing = false;
}

TEST(insert_closer_at_end_appends) {
  EnableAutoPairing = true;

  char buf[16] = "ab";
  size_t cur = strlen(buf);
  EditBufferInsertText(buf, sizeof(buf), &cur, "}", 1);
  ASSERT_EQ_STR(buf, "ab}");
  ASSERT_EQ_INT(cur, 3);

  EnableAutoPairing = false;
}

TEST(backspace_pair_deletes_both) {
  EnableAutoPairing = true;

  char buf[8] = "()";
  size_t cur = 1;
  ASSERT_TRUE(EditBufferBackspacePair(buf, &cur));
  ASSERT_EQ_STR(buf, "");
  ASSERT_EQ_INT(cur, 0);

  EnableAutoPairing = false;
}

TEST(backspace_pair_disabled_or_empty) {
  EnableAutoPairing = false;

  char b2[8] = "()";
  size_t c2 = 2;
  ASSERT_FALSE(EditBufferBackspacePair(b2, &c2));

  EnableAutoPairing = true;
  char b3[8] = "";
  size_t c3 = 0;
  ASSERT_FALSE(EditBufferBackspacePair(b3, &c3));

  EnableAutoPairing = false;
}

int main(void) {
  RUN(backspace_removes_before_cursor);
  RUN(backspace_at_zero_is_noop);
  RUN(delete_char_middle_and_ends);
  RUN(cursor_movement_bounds);
  RUN(pair_for_round_pairs);
  RUN(insert_into_middle);
  RUN(insert_respects_capacity);
  RUN(insert_autopair_opener);
  RUN(insert_quote_after_letter_is_not_paired);
  RUN(insert_skips_over_closer);
  RUN(insert_closer_at_end_appends);
  RUN(backspace_pair_deletes_both);
  RUN(backspace_pair_disabled_or_empty);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
