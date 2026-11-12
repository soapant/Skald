#include "test.h"
#include <stdio.h>
#include <string.h>
#include "core/inline_edit.h"
#include <string.h>

TEST(inline_edit_singleton_shared_buffer) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  EditBoxInfo *a = InlineEdit_Find(&state, "name");
  ASSERT_EQ_INT(a == NULL, 1);

  EditBoxInfo *b = InlineEdit_FindOrCreate(&state, "name");
  ASSERT_EQ_INT(b != NULL, 1);
  EditBoxInfo *c = InlineEdit_FindOrCreate(&state, "name");
  ASSERT_EQ_INT(b, c);
  ASSERT_EQ_INT(state.inline_edits.count, 1);
}

TEST(inline_edit_multiple_names_independent_text) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  InlineEdit_SetText(&state, "one", "hello");
  InlineEdit_SetText(&state, "two", "world");
  ASSERT_EQ_STR(InlineEdit_Text(&state, "one"), "hello");
  ASSERT_EQ_STR(InlineEdit_Text(&state, "two"), "world");
}

TEST(inline_edit_activate_switches_target) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  InlineEdit_SetText(&state, "first", "aaa");
  InlineEdit_SetText(&state, "second", "bbb");

  InlineEdit_Activate(&state, "second");
  ASSERT_EQ_INT(InlineEdit_IsActiveName(&state, "second"), 1);
  ASSERT_EQ_INT(InlineEdit_IsActiveName(&state, "first"), 0);

  EditBoxInfo *active = InlineEdit_Active(&state);
  ASSERT_EQ_INT(active == InlineEdit_Find(&state, "second"), 1);

  InlineEdit_Deactivate(&state);
  ASSERT_EQ_INT(InlineEdit_Active(&state) == NULL, 1);
}

TEST(inline_edit_set_text_truncates_to_buffer) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  char big[1024];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';
  InlineEdit_SetText(&state, "a", big);
  ASSERT_EQ_INT(strlen(InlineEdit_Text(&state, "a")),
                sizeof(((EditBoxInfo *)0)->filter) - 1);
}

TEST(inline_edit_placeholder_text_survives_refresh) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  EditBoxInfo *p = InlineEdit_FindOrCreate(&state, "mail");
  ASSERT_EQ_INT(p != NULL, 1);
  p->placeholder = strdup("you@example.com");
  InlineEdit_SetText(&state, "mail", "");
  ASSERT_EQ_STR(p->placeholder, "you@example.com");
  InlineEdit_Reset(&state);
  ASSERT_EQ_INT(state.inline_edits.count, 0);
}

TEST(inline_edit_max_entries_rejected) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  state.inline_edits.active = -1;

  for (int i = 0; i < INLINE_EDIT_MAX; i++) {
    char name[8];
    snprintf(name, sizeof(name), "k%d", i);
    ASSERT_EQ_INT(InlineEdit_FindOrCreate(&state, name) != NULL, 1);
  }
  ASSERT_EQ_INT(InlineEdit_FindOrCreate(&state, "overflow") == NULL, 1);
}

int main(void) {
  RUN(inline_edit_singleton_shared_buffer);
  RUN(inline_edit_multiple_names_independent_text);
  RUN(inline_edit_activate_switches_target);
  RUN(inline_edit_set_text_truncates_to_buffer);
  RUN(inline_edit_placeholder_text_survives_refresh);
  RUN(inline_edit_max_entries_rejected);
  return TEST_SUMMARY();
}
