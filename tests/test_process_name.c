#include <stdio.h>
#include <string.h>

#include "test.h"

#include "helpers/helpers_misc.h"

TEST(accepts_plain_process_names) {
  ASSERT_TRUE(is_safe_process_name("firefox"));
  ASSERT_TRUE(is_safe_process_name("chrome.exe"));
  ASSERT_TRUE(is_safe_process_name("my-app_2"));
  ASSERT_TRUE(is_safe_process_name("C"));
}

TEST(rejects_shell_metacharacters) {
  ASSERT_FALSE(is_safe_process_name("$(rm -rf /)"));
  ASSERT_FALSE(is_safe_process_name("a;b"));
  ASSERT_FALSE(is_safe_process_name("a|b"));
  ASSERT_FALSE(is_safe_process_name("a&&b"));
  ASSERT_FALSE(is_safe_process_name("a b"));
  ASSERT_FALSE(is_safe_process_name("a\"b"));
  ASSERT_FALSE(is_safe_process_name("a'b"));
  ASSERT_FALSE(is_safe_process_name("a`b"));
  ASSERT_FALSE(is_safe_process_name("a\\b"));
  ASSERT_FALSE(is_safe_process_name("a/b"));
  ASSERT_FALSE(is_safe_process_name("a$b"));
  ASSERT_FALSE(is_safe_process_name("a*b"));
}

TEST(rejects_empty_and_null) {
  ASSERT_FALSE(is_safe_process_name(""));
  ASSERT_FALSE(is_safe_process_name(NULL));
}

int main(void) {
  RUN(accepts_plain_process_names);
  RUN(rejects_shell_metacharacters);
  RUN(rejects_empty_and_null);
  return TEST_SUMMARY();
}
