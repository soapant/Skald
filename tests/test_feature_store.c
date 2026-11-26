#include "test.h"
#include "test_tmp.h"
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
  ASSERT_TRUE(sk_store_clear());
  ASSERT_TRUE(sk_store_set("alpha"));
  char *last = sk_store_get_last();
  ASSERT_EQ_STR(last, "alpha");
  free(last);
  SkStringList all = sk_store_get_all();
  ASSERT_EQ_INT(all.count, 1);
  ASSERT_EQ_STR(all.items[0], "alpha");
  sk_free_string_list(&all);
  ASSERT_TRUE(sk_store_clear());
  all = sk_store_get_all();
  ASSERT_EQ_INT(all.count, 0);
  sk_free_string_list(&all);
}

TEST(store_append_and_replace_all) {
  ASSERT_TRUE(sk_store_clear());
  ASSERT_TRUE(sk_store_append("one"));
  ASSERT_TRUE(sk_store_append("two"));
  SkStringList all = sk_store_get_all();
  ASSERT_EQ_INT(all.count, 2);
  ASSERT_EQ_STR(all.items[0], "one");
  ASSERT_EQ_STR(all.items[1], "two");
  sk_free_string_list(&all);
  const char *replacement[] = {"beta", "gamma"};
  ASSERT_TRUE(sk_store_replace_all(replacement, 2));
  all = sk_store_get_all();
  ASSERT_EQ_INT(all.count, 2);
  ASSERT_EQ_STR(all.items[0], "beta");
  ASSERT_EQ_STR(all.items[1], "gamma");
  sk_free_string_list(&all);
  ASSERT_TRUE(sk_store_clear());
}

TEST(store_rejects_newlines) {
  ASSERT_FALSE(sk_store_append("bad\nvalue"));
  ASSERT_FALSE(sk_store_set("bad\rvalue"));
  ASSERT_TRUE(sk_store_clear());
}

TEST(store_path_uses_source_file_stem) {
  ASSERT_TRUE(sk_store_set("x"));
  char base[512];
  ASSERT_TRUE(data_dir(base, sizeof(base)));
  char expected[1024];
  int n = snprintf(expected, sizeof(expected),
                   "%s%stest_feature_store%stest_feature_store.txt", base,
                   STORE_TEST_SEP, STORE_TEST_SEP);
  ASSERT_TRUE(n >= 0 && (size_t)n < sizeof(expected));
  ASSERT_TRUE(sk_file_exists(expected));
  ASSERT_TRUE(sk_store_clear());
}

static char STORE_TMP[512];

static void store_cleanup(void) {
  test_tmp_remove_tree(STORE_TMP);
  data_dir_set_override(NULL);
}

int main(void) {
  test_tmp_path(STORE_TMP, sizeof(STORE_TMP), "feature_store");
  test_tmp_make(STORE_TMP);
  data_dir_set_override(STORE_TMP);
  TEST_GLOBAL_TEARDOWN(store_cleanup);
  RUN(store_set_get_last_clear);
  RUN(store_append_and_replace_all);
  RUN(store_rejects_newlines);
  RUN(store_path_uses_source_file_stem);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
