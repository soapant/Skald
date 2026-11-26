#include "test.h"
#include "core/skapi.h"
#include "helpers/helpers_fs.h"
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32)
#define STORE_TEST_SEP "\\"
#else
#define STORE_TEST_SEP "/"
#endif

TEST(store_set_get_last_clear) {
  const char *key = "sk_store_test_set";
  ASSERT_TRUE(sk_store_clear(key));
  ASSERT_TRUE(sk_store_set(key, "alpha"));
  char *last = sk_store_get_last(key);
  ASSERT_EQ_STR(last, "alpha");
  free(last);
  SkStringList all = sk_store_get_all(key);
  ASSERT_EQ_INT(all.count, 1);
  ASSERT_EQ_STR(all.items[0], "alpha");
  sk_free_string_list(&all);
  ASSERT_TRUE(sk_store_clear(key));
  all = sk_store_get_all(key);
  ASSERT_EQ_INT(all.count, 0);
  sk_free_string_list(&all);
}

TEST(store_append_and_replace_all) {
  const char *key = "sk_store_test_append";
  ASSERT_TRUE(sk_store_clear(key));
  ASSERT_TRUE(sk_store_append(key, "one"));
  ASSERT_TRUE(sk_store_append(key, "two"));
  SkStringList all = sk_store_get_all(key);
  ASSERT_EQ_INT(all.count, 2);
  ASSERT_EQ_STR(all.items[0], "one");
  ASSERT_EQ_STR(all.items[1], "two");
  sk_free_string_list(&all);
  const char *replacement[] = {"beta", "gamma"};
  ASSERT_TRUE(sk_store_replace_all(key, replacement, 2));
  all = sk_store_get_all(key);
  ASSERT_EQ_INT(all.count, 2);
  ASSERT_EQ_STR(all.items[0], "beta");
  ASSERT_EQ_STR(all.items[1], "gamma");
  sk_free_string_list(&all);
  ASSERT_TRUE(sk_store_clear(key));
}

TEST(store_rejects_newlines) {
  const char *key = "sk_store_test_newline";
  ASSERT_FALSE(sk_store_append(key, "bad\nvalue"));
  ASSERT_FALSE(sk_store_set(key, "bad\rvalue"));
  ASSERT_TRUE(sk_store_clear(key));
}

TEST(store_path_uses_source_file_stem) {
  const char *key = "sk_store_test_path";
  ASSERT_TRUE(sk_store_set(key, "x"));
  char base[512];
  ASSERT_TRUE(data_dir(base, sizeof(base)));
  char expected[1024];
  int n = snprintf(expected, sizeof(expected),
                   "%s%sstores%stest_feature_store%s%s.txt", base,
                   STORE_TEST_SEP, STORE_TEST_SEP, STORE_TEST_SEP, key);
  ASSERT_TRUE(n >= 0 && (size_t)n < sizeof(expected));
  ASSERT_TRUE(sk_file_exists(expected));
  ASSERT_TRUE(sk_store_clear(key));
}

int main(void) {
  RUN(store_set_get_last_clear);
  RUN(store_append_and_replace_all);
  RUN(store_rejects_newlines);
  RUN(store_path_uses_source_file_stem);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
