#include <stdio.h>
#include <string.h>

#include "test.h"

#include "helpers/helpers_misc.h"

TEST(accepts_plain_process_names) {
  ASSERT_TRUE(IsSafeProcessName("firefox"));
  ASSERT_TRUE(IsSafeProcessName("chrome.exe"));
  ASSERT_TRUE(IsSafeProcessName("my-app_2"));
  ASSERT_TRUE(IsSafeProcessName("C"));
}

TEST(rejects_shell_metacharacters) {
  ASSERT_FALSE(IsSafeProcessName("$(rm -rf /)"));
  ASSERT_FALSE(IsSafeProcessName("a;b"));
  ASSERT_FALSE(IsSafeProcessName("a|b"));
  ASSERT_FALSE(IsSafeProcessName("a&&b"));
  ASSERT_FALSE(IsSafeProcessName("a b"));
  ASSERT_FALSE(IsSafeProcessName("a\"b"));
  ASSERT_FALSE(IsSafeProcessName("a'b"));
  ASSERT_FALSE(IsSafeProcessName("a`b"));
  ASSERT_FALSE(IsSafeProcessName("a\\b"));
  ASSERT_FALSE(IsSafeProcessName("a/b"));
  ASSERT_FALSE(IsSafeProcessName("a$b"));
  ASSERT_FALSE(IsSafeProcessName("a*b"));
}

TEST(rejects_empty_and_null) {
  ASSERT_FALSE(IsSafeProcessName(""));
  ASSERT_FALSE(IsSafeProcessName(NULL));
}

int main(void) {
  RUN(accepts_plain_process_names);
  RUN(rejects_shell_metacharacters);
  RUN(rejects_empty_and_null);
  return TEST_SUMMARY();
}
