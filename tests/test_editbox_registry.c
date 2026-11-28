#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/editbox.h"
#include "core/focus.h"
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
  int idx =
      editbox_registry_place(&(EditboxRegistryPlace){.state = &state,
                                                     .name = "a",
                                                     .buffer = buf,
                                                     .buffer_size = sizeof(buf),
                                                     .x = 0,
                                                     .y = 0,
                                                     .w = 10,
                                                     .h = 10});
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

TEST(editbox_registry_syncs_all_boxes) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  EditBox legacy;
  memset(&legacy, 0, sizeof(legacy));
  state.default_eb = &legacy;
  state.eb = &legacy;
  state.active_editbox = -1;

  char first[16] = "a";
  char second[16] = "b";
  int first_idx = editbox_registry_place(
      &(EditboxRegistryPlace){.state = &state,
                              .name = "first",
                              .buffer = first,
                              .buffer_size = sizeof(first),
                              .x = 0,
                              .y = 0,
                              .w = 10,
                              .h = 10});
  int second_idx = editbox_registry_place(
      &(EditboxRegistryPlace){.state = &state,
                              .name = "second",
                              .buffer = second,
                              .buffer_size = sizeof(second),
                              .x = 20,
                              .y = 0,
                              .w = 10,
                              .h = 10});
  ASSERT_TRUE(first_idx >= 0);
  ASSERT_TRUE(second_idx >= 0);

  strcpy(state.editboxes[first_idx].eb.filter, "aa");
  strcpy(state.editboxes[second_idx].eb.filter, "bb");
  state.active_editbox = second_idx;
  editbox_registry_sync(&state);
  ASSERT_EQ_STR(first, "aa");
  ASSERT_EQ_STR(second, "bb");
}

TEST(editbox_registry_participates_in_focus_ring) {
  PromptState state;
  memset(&state, 0, sizeof(state));
  EditBox legacy;
  memset(&legacy, 0, sizeof(legacy));
  state.default_eb = &legacy;
  state.eb = &legacy;
  state.active_editbox = -1;

  char first[16] = "a";
  char second[16] = "b";
  int first_idx = editbox_registry_place(
      &(EditboxRegistryPlace){.state = &state,
                              .name = "first",
                              .buffer = first,
                              .buffer_size = sizeof(first),
                              .x = 0,
                              .y = 0,
                              .w = 10,
                              .h = 10});
  ASSERT_TRUE(first_idx >= 0);
  int second_idx = editbox_registry_place(
      &(EditboxRegistryPlace){.state = &state,
                              .name = "second",
                              .buffer = second,
                              .buffer_size = sizeof(second),
                              .x = 20,
                              .y = 0,
                              .w = 10,
                              .h = 10});
  ASSERT_TRUE(second_idx >= 0);
  state.editboxes[first_idx].no_tab = true;

  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){
      .include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  editbox_registry_register_focus(&state);
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "editbox:first");
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "editbox:second");
  ASSERT_TRUE(state.eb == &state.editboxes[second_idx].eb);

  editbox_registry_set_active(&state, "first");
  ASSERT_TRUE(focus_is("editbox:first"));
  focus_clear();
}

int main(void) {
  RUN(editbox_registry_routes_active_box);
  RUN(editbox_registry_syncs_all_boxes);
  RUN(editbox_registry_participates_in_focus_ring);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
