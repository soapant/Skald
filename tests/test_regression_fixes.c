#include "test.h"
#include "test_tmp.h"
#include <stdatomic.h>
#include <stdio.h>
#include "config_values.h"
#include "core/context.h"
#include "core/owner_purge.h"
#include "core/prompt_geometry.h"
#include "core/reload_lib.h"
#include "helpers/helpers_ui.h"

static char REGRESSION_TMP[512];

static void regression_cleanup(void) {
  window_set_current_feature(NULL);
  test_tmp_remove_tree(REGRESSION_TMP);
}

TEST(window_set_current_feature_copies) {
  char stack_name[64] = "Todo";
  window_set_current_feature(stack_name);
  memset(stack_name, 0, sizeof(stack_name));
  ASSERT_EQ_STR(SkaldContext_Get()->window.current_feature, "Todo");
  window_set_current_feature("Other");
  ASSERT_EQ_STR(SkaldContext_Get()->window.current_feature, "Other");
  window_set_current_feature(NULL);
  ASSERT_TRUE(SkaldContext_Get()->window.current_feature == NULL);
}

TEST(content_hash_detects_change) {
  char a[512];
  char b[512];
  snprintf(a, sizeof(a), "%s/a.bin", REGRESSION_TMP);
  snprintf(b, sizeof(b), "%s/b.bin", REGRESSION_TMP);
  FILE *fa = fopen(a, "wb");
  ASSERT_TRUE(fa != NULL);
  fwrite("abcd", 1, 4, fa);
  fclose(fa);
  FILE *fb = fopen(b, "wb");
  ASSERT_TRUE(fb != NULL);
  fwrite("abce", 1, 4, fb);
  fclose(fb);
  unsigned long long ha = 0, hb = 0;
  ASSERT_TRUE(reload_lib_content_hash(a, &ha));
  ASSERT_TRUE(reload_lib_content_hash(b, &hb));
  ASSERT_TRUE(ha != hb);
  ASSERT_TRUE(reload_lib_content_hash(a, &hb));
  ASSERT_EQ_INT(ha, hb);
}

static int purge_hook_calls;
static unsigned purge_hook_gen;

static void test_purge_hook(const char *a, const char *b, unsigned gen) {
  (void)a;
  (void)b;
  purge_hook_calls++;
  purge_hook_gen = gen;
}

TEST(owner_purge_registry_invokes_hooks) {
  ASSERT_TRUE(owner_purge_register(test_purge_hook));
  purge_hook_calls = 0;
  purge_hook_gen = 0;
  purge_owner_set("lib_a", "lib_b", 17);
  ASSERT_EQ_INT(purge_hook_calls, 1);
  ASSERT_EQ_INT(purge_hook_gen, 17);
}

TEST(library_track_remove_checked) {
  void *fake = (void *)&purge_hook_calls;
  library_track_add(fake, "mod_a");
  ASSERT_EQ_INT(library_track_remove_checked(fake, "mod_a"),
                LIB_TRACK_SELF_CALLER);
  ASSERT_EQ_INT(library_track_remove_checked(fake, "mod_b"), LIB_TRACK_REMOVED);
  ASSERT_EQ_INT(library_track_remove_checked(fake, "mod_b"),
                LIB_TRACK_NOT_FOUND);
}

TEST(metrics_get_tracks_font_size) {
  int saved = atomic_load_explicit(&FontSize, memory_order_relaxed);
  atomic_store_explicit(&FontSize, saved + 3, memory_order_relaxed);
  const Metrics *m = metrics_get();
  ASSERT_EQ_INT(m->listbox_item_height, prompt_row_height());
  ASSERT_EQ_INT(m->edit_cursor_height, prompt_caret_height());
  atomic_store_explicit(&FontSize, saved, memory_order_relaxed);
  (void)metrics_get();
}

int main(void) {
  test_tmp_path(REGRESSION_TMP, sizeof(REGRESSION_TMP), "regression_fixes");
  test_tmp_make(REGRESSION_TMP);
  TEST_SUITE_TEARDOWN(regression_cleanup);
  RUN(window_set_current_feature_copies);
  RUN(content_hash_detects_change);
  RUN(owner_purge_registry_invokes_hooks);
  RUN(library_track_remove_checked);
  RUN(metrics_get_tracks_font_size);
  return TEST_SUMMARY();
}
