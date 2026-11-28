#include "test.h"
#include "test_tmp.h"
#include <sys/stat.h>
#include <stdio.h>
#include "core/context.h"
#include "core/feature_path.h"
#include "helpers/helpers_fs.h"
#include "platform/platform.h"

#if defined(_WIN32)
#define TEST_SEP "\\"
#else
#define TEST_SEP "/"
#endif

static char RESOLVE_TMP[512];

static void resolve_cleanup(void) {
  window_set_current_feature(NULL);
  test_tmp_remove_tree(RESOLVE_TMP);
  data_dir_set_override(NULL);
}

static void test_data_dir(char *out, size_t n) { data_dir(out, n); }

static void test_stem_dir(const char *stem, char *out, size_t n) {
  char base[512];
  test_data_dir(base, sizeof(base));
  snprintf(out, n, "%s%s%s", base, TEST_SEP, stem);
}

TEST(existing_behavior_preserved) {
  char dir[512];
  test_stem_dir("todo", dir, sizeof(dir));
  test_tmp_make(dir);
  window_set_current_feature("Todo");
  char out[512] = {0};
  bool ok = feature_path_resolve_data_path(__builtin_return_address(0),
                                           "todo.txt", out, sizeof(out));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(strstr(out, "todo" TEST_SEP "todo.txt") != NULL);
  Platform_RemoveDirectory(dir);
}

TEST(unknown_still_fails) {
  window_set_current_feature(NULL);
  char out[512] = {0};
  bool ok = feature_path_resolve_data_path(
      __builtin_return_address(0), "nosuchfeaturexyz.txt", out, sizeof(out));
  ASSERT_FALSE(ok);
}

TEST(foreground_first_save) {
  char dir[512];
  test_stem_dir("clipboard_history", dir, sizeof(dir));
  test_tmp_make(dir);
  window_set_current_feature("Clipboard History");
  char out[512] = {0};
  bool ok = feature_path_resolve_data_path(__builtin_return_address(0),
                                           "zzfirstsave.txt", out, sizeof(out));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(strstr(out, "clipboard_history" TEST_SEP "zzfirstsave.txt") !=
              NULL);
  Platform_RemoveDirectory(dir);
}

int main(void) {
  test_tmp_path(RESOLVE_TMP, sizeof(RESOLVE_TMP), "resolve");
  test_tmp_make(RESOLVE_TMP);
  data_dir_set_override(RESOLVE_TMP);
  TEST_GLOBAL_TEARDOWN(resolve_cleanup);
  RUN(existing_behavior_preserved);
  RUN(unknown_still_fails);
  RUN(foreground_first_save);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
