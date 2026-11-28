#include "test.h"
#include "core/focus.h"

static int s_activated = 0;
static int s_adjusted = 0;
static int s_adjust_dir = 0;

static void focus_test_activate(void *owner) {
  (void)owner;
  s_activated++;
}

static void focus_test_adjust(void *owner, int dir) {
  (void)owner;
  s_adjusted++;
  s_adjust_dir = dir;
}

TEST(focus_move_cycles_widgets) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "b2", .kind = FOCUS_KIND_BUTTON, .x = 20, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "sl", .kind = FOCUS_KIND_SLIDER, .x = 40, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_has_entries());
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "b1");
  focus_move(1);
  ASSERT_EQ_STR(focus_name(), "b2");
  focus_move(1);
  ASSERT_EQ_STR(focus_name(), "sl");
  focus_move(1);
  ASSERT_EQ_STR(focus_name(), "b1");
  focus_move(-1);
  ASSERT_EQ_STR(focus_name(), "sl");
}

TEST(focus_activate_invokes_callback) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  s_activated = 0;
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = focus_test_activate, .adjust = NULL});
  ASSERT_TRUE(focus_select("b1"));
  ASSERT_TRUE(focus_activate());
  ASSERT_EQ_INT(s_activated, 1);
}

TEST(focus_adjust_invokes_callback) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  s_adjusted = 0;
  s_adjust_dir = 0;
  focus_register(&(FocusRegister){.name = "sl", .kind = FOCUS_KIND_SLIDER, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = focus_test_adjust});
  ASSERT_TRUE(focus_select("sl"));
  ASSERT_TRUE(focus_adjust(-1));
  ASSERT_EQ_INT(s_adjusted, 1);
  ASSERT_EQ_INT(s_adjust_dir, -1);
}

TEST(focus_register_restores_selection_for_draw) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_select("b1"));
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_is("b1"));
}

TEST(focus_frame_end_drops_missing_widget) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_select("b1");
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_BUTTON);
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b2", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_NONE);
}

TEST(focus_pseudo_listbox_survives_frame) {
  focus_clear();
  focus_set_pseudo(FOCUS_KIND_LISTBOX);
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
  ASSERT_FALSE(focus_is("b1"));
}

TEST(focus_listbox_is_part_of_ring) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 0, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "b1");
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  focus_frame_end();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 0, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_EQ_STR(focus_name(), "__listbox__");
}

TEST(focus_passive_listbox_selects_ring_entry) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 0, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_select("b1"));
  focus_set_pseudo_passive(FOCUS_KIND_LISTBOX);
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
}

TEST(focus_move_direction_picks_axis_neighbor) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "center", .kind = FOCUS_KIND_BUTTON, .x = 50, .y = 50, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "left", .kind = FOCUS_KIND_CHECKBOX, .x = 0, .y = 50, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "right", .kind = FOCUS_KIND_SLIDER, .x = 100, .y = 50, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "top", .kind = FOCUS_KIND_CUSTOM, .x = 50, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "bottom", .kind = FOCUS_KIND_BUTTON, .x = 50, .y = 100, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_select("center"));

  ASSERT_TRUE(focus_move_direction(FOCUS_LEFT));
  ASSERT_EQ_STR(focus_name(), "left");
  ASSERT_TRUE(focus_select("center"));
  ASSERT_TRUE(focus_move_direction(FOCUS_RIGHT));
  ASSERT_EQ_STR(focus_name(), "right");
  ASSERT_TRUE(focus_select("center"));
  ASSERT_TRUE(focus_move_direction(FOCUS_UP));
  ASSERT_EQ_STR(focus_name(), "top");
  ASSERT_TRUE(focus_select("center"));
  ASSERT_TRUE(focus_move_direction(FOCUS_DOWN));
  ASSERT_EQ_STR(focus_name(), "bottom");
}

TEST(focus_set_kind_selects_first_match) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = false, .lb_x = 0, .lb_y = 0, .lb_w = 0, .lb_h = 0});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "c1", .kind = FOCUS_KIND_CHECKBOX, .x = 20, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_TRUE(focus_set_kind(FOCUS_KIND_CHECKBOX));
  ASSERT_EQ_STR(focus_name(), "c1");
  ASSERT_TRUE(focus_set_kind(FOCUS_KIND_NONE));
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_NONE);
}

TEST(focus_pseudo_editbox_survives_listbox_frame) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 0, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_set_pseudo(FOCUS_KIND_EDITBOX);
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_EDITBOX);
  focus_frame_end();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 0, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "b1", .kind = FOCUS_KIND_BUTTON, .x = 0, .y = 0, .w = 10, .h = 10, .owner = NULL, .activate = NULL, .adjust = NULL});
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_EDITBOX);
  ASSERT_TRUE(focus_name() == NULL);
}

TEST(focus_editbox_down_selects_listbox) {
  focus_clear();
  focus_frame_begin(&(FocusFrameBegin){.include_listbox = true, .lb_x = 0, .lb_y = 30, .lb_w = 100, .lb_h = 100});
  focus_register(&(FocusRegister){.name = "eb", .kind = FOCUS_KIND_EDITBOX, .x = 0, .y = 0, .w = 100, .h = 20, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_register(&(FocusRegister){.name = "btn", .kind = FOCUS_KIND_BUTTON, .x = 70, .y = 200, .w = 30, .h = 20, .owner = NULL, .activate = NULL, .adjust = NULL});
  focus_frame_end();
  ASSERT_TRUE(focus_select("btn"));
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "eb");
  ASSERT_TRUE(focus_move_direction(FOCUS_DOWN));
  ASSERT_EQ_STR(focus_name(), "__listbox__");
}

int main(void) {
  RUN(focus_move_cycles_widgets);
  RUN(focus_activate_invokes_callback);
  RUN(focus_adjust_invokes_callback);
  RUN(focus_register_restores_selection_for_draw);
  RUN(focus_frame_end_drops_missing_widget);
  RUN(focus_pseudo_listbox_survives_frame);
  RUN(focus_listbox_is_part_of_ring);
  RUN(focus_passive_listbox_selects_ring_entry);
  RUN(focus_move_direction_picks_axis_neighbor);
  RUN(focus_set_kind_selects_first_match);
  RUN(focus_pseudo_editbox_survives_listbox_frame);
  RUN(focus_editbox_down_selects_listbox);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}

