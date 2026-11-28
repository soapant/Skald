#include "test.h"
#include "core/snippet_manager.h"
#include "platform/platform.h"
#include <stdlib.h>

static void dummy(void) {}

static const SnippetBinding *find_by_trigger(const char *t) {
  int n = snippet_binding_count();
  for (int i = 0; i < n; i++) {
    const SnippetBinding *b = snippet_get_binding(i);
    if (b && b->trigger && strcmp(b->trigger, t) == 0)
      return b;
  }
  return NULL;
}

static int count_of(const char *t) {
  int n = snippet_binding_count(), c = 0;
  for (int i = 0; i < n; i++) {
    const SnippetBinding *b = snippet_get_binding(i);
    if (b && b->trigger && strcmp(b->trigger, t) == 0)
      c++;
  }
  return c;
}

TEST(register_then_reregister_same_trigger_survives_match) {
  snippet_reset_buffer();
  ASSERT_TRUE(snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "jq", .f_text = NULL, .f_func = dummy, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL}));
  ASSERT_TRUE(snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "jq", .f_text = NULL, .f_func = dummy, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL}));
  ASSERT_EQ_INT(count_of("jq"), 1);
  const SnippetBinding *b = find_by_trigger("jq");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  ASSERT_TRUE(b->trigger != NULL);
}

TEST(repeated_reregister_does_not_leak_null_trigger) {
  snippet_reset_buffer();
  for (int i = 0; i < 50; i++) {
    snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "zz", .f_text = "text", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  }
  ASSERT_EQ_INT(count_of("zz"), 1);
  const SnippetBinding *b = find_by_trigger("zz");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  ASSERT_TRUE(b->trigger != NULL);
}

TEST(reregister_can_switch_text_to_func) {
  snippet_reset_buffer();
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "sw", .f_text = "hello", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  ASSERT_EQ_INT(count_of("sw"), 1);
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "sw", .f_text = NULL, .f_func = dummy, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  ASSERT_EQ_INT(count_of("sw"), 1);
  const SnippetBinding *b = find_by_trigger("sw");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  if (b->trigger)
    ASSERT_EQ_STR(b->trigger, "sw");
  ASSERT_TRUE(b->text == NULL);
  ASSERT_TRUE(b->func != NULL);
}

TEST(reregister_can_switch_func_to_text) {
  snippet_reset_buffer();
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "sf", .f_text = NULL, .f_func = dummy, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  ASSERT_EQ_INT(count_of("sf"), 1);
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "sf", .f_text = "hello", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  ASSERT_EQ_INT(count_of("sf"), 1);
  const SnippetBinding *b = find_by_trigger("sf");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  if (b->trigger)
    ASSERT_EQ_STR(b->trigger, "sf");
  ASSERT_TRUE(b->func == NULL);
  ASSERT_TRUE(b->text != NULL);
  if (b->text)
    ASSERT_EQ_STR(b->text, "hello");
}

TEST(distinct_triggers_do_not_collapse) {
  snippet_reset_buffer();
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "aa", .f_text = "1", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "bb", .f_text = "2", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "cc", .f_text = "3", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL});
  ASSERT_EQ_INT(count_of("aa"), 1);
  ASSERT_EQ_INT(count_of("bb"), 1);
  ASSERT_EQ_INT(count_of("cc"), 1);
}

TEST(reregister_preserves_window_whitelist) {
  snippet_reset_buffer();
  const char *wins[] = {"feh", NULL};
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "ww", .f_text = "1", .f_func = NULL, .f_whitelist = true, .f_windows = wins, .f_caller = NULL});
  const SnippetBinding *b = find_by_trigger("ww");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  ASSERT_EQ_INT(b->window_count, 1);
  if (b->window_count == 1 && b->windows[0])
    ASSERT_EQ_STR(b->windows[0], "feh");
}

TEST(reregister_replaces_window_list_without_leaking) {
  snippet_reset_buffer();
  const char *w1[] = {"feh", NULL};
  const char *w2[] = {"kitty", NULL};
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "wx", .f_text = "1", .f_func = NULL, .f_whitelist = false, .f_windows = w1, .f_caller = NULL});
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "wx", .f_text = "1", .f_func = NULL, .f_whitelist = false, .f_windows = w2, .f_caller = NULL});
  const SnippetBinding *b = find_by_trigger("wx");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  ASSERT_EQ_INT(b->window_count, 1);
  if (b->window_count == 1 && b->windows[0])
    ASSERT_EQ_STR(b->windows[0], "kitty");
}

TEST(get_binding_rejects_out_of_range) {
  snippet_reset_buffer();
  ASSERT_TRUE(snippet_get_binding(-1) == NULL);
  ASSERT_TRUE(snippet_get_binding(snippet_binding_count()) == NULL);
}

TEST(rejects_invalid_registration) {
  ASSERT_FALSE(snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = NULL, .f_text = "x", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL}));
  ASSERT_FALSE(snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "", .f_text = "x", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL}));
  ASSERT_FALSE(snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "tt", .f_text = NULL, .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = NULL}));
}

TEST(owner_falls_back_to_caller_when_no_func) {
  snippet_reset_buffer();
  snippet_register_internal(&(FnSnippetRegisterInternalSpec){.f_trigger = "own", .f_text = "x", .f_func = NULL, .f_whitelist = false, .f_windows = NULL, .f_caller = (void *)dummy});
  const SnippetBinding *b = find_by_trigger("own");
  ASSERT_TRUE(b != NULL);
  if (!b)
    return;
  ASSERT_TRUE(b->tag.owner && b->tag.owner[0]);
}

int main(void) {
  RUN(register_then_reregister_same_trigger_survives_match);
  RUN(repeated_reregister_does_not_leak_null_trigger);
  RUN(reregister_can_switch_text_to_func);
  RUN(reregister_can_switch_func_to_text);
  RUN(distinct_triggers_do_not_collapse);
  RUN(reregister_preserves_window_whitelist);
  RUN(reregister_replaces_window_list_without_leaking);
  RUN(get_binding_rejects_out_of_range);
  RUN(rejects_invalid_registration);
  RUN(owner_falls_back_to_caller_when_no_func);
  return TEST_SUMMARY();
}
