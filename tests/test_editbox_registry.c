#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/editbox.h"
#include "core/types/prompt.h"

TEST(editbox_registry_routes_active_box) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  EditBox legacy;
  memset(&legacy, 0, sizeof(legacy));
  state.default_eb = &legacy;
  state.eb = &legacy;
  state.active_editbox = -1;

  char buf[16] = "hello";
  int idx = editbox_registry_place(&state, "a", buf, sizeof(buf), 0, 0, 10, 10);
  ASSERT_TRUE(idx >= 0);
  PromptEditbox *entry = editbox_registry_find(&state, "a");
  ASSERT_TRUE(entry != NULL);
  ASSERT_EQ_STR(entry->eb.filter, "hello");

  editbox_registry_set_active(&state, "a");
  ASSERT_TRUE(state.eb == &state.editboxes[idx].eb);

  strcpy(state.eb->filter, "world");
  state.eb->cursor = 5;
  editbox_registry_sync(&state);
  ASSERT_EQ_STR(buf, "world");

  editbox_registry_set_active(&state, NULL);
  ASSERT_TRUE(state.eb == &legacy);
}

int main(void) {
  RUN(editbox_registry_routes_active_box);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
