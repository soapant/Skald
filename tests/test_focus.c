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
  focus_frame_begin(false);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  focus_register("b2", FOCUS_KIND_BUTTON, 20, 0, 10, 10, NULL, NULL, NULL);
  focus_register("sl", FOCUS_KIND_SLIDER, 40, 0, 10, 10, NULL, NULL, NULL);
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
  focus_frame_begin(false);
  s_activated = 0;
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL,
                 focus_test_activate, NULL);
  ASSERT_TRUE(focus_select("b1"));
  ASSERT_TRUE(focus_activate());
  ASSERT_EQ_INT(s_activated, 1);
}

TEST(focus_adjust_invokes_callback) {
  focus_clear();
  focus_frame_begin(false);
  s_adjusted = 0;
  s_adjust_dir = 0;
  focus_register("sl", FOCUS_KIND_SLIDER, 0, 0, 10, 10, NULL, NULL,
                 focus_test_adjust);
  ASSERT_TRUE(focus_select("sl"));
  ASSERT_TRUE(focus_adjust(-1));
  ASSERT_EQ_INT(s_adjusted, 1);
  ASSERT_EQ_INT(s_adjust_dir, -1);
}

TEST(focus_register_restores_selection_for_draw) {
  focus_clear();
  focus_frame_begin(false);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  ASSERT_TRUE(focus_select("b1"));
  focus_frame_begin(false);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  ASSERT_TRUE(focus_is("b1"));
}

TEST(focus_frame_end_drops_missing_widget) {
  focus_clear();
  focus_frame_begin(false);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  focus_select("b1");
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_BUTTON);
  focus_frame_begin(false);
  focus_register("b2", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_NONE);
}

TEST(focus_pseudo_listbox_survives_frame) {
  focus_clear();
  focus_set_pseudo(FOCUS_KIND_LISTBOX);
  focus_frame_begin(false);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  focus_frame_end();
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
  ASSERT_FALSE(focus_is("b1"));
}

TEST(focus_listbox_is_part_of_ring) {
  focus_clear();
  focus_frame_begin(true);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "b1");
  ASSERT_TRUE(focus_move(1));
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  focus_frame_end();
  focus_frame_begin(true);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  ASSERT_EQ_STR(focus_name(), "__listbox__");
}

TEST(focus_passive_listbox_selects_ring_entry) {
  focus_clear();
  focus_frame_begin(true);
  focus_register("b1", FOCUS_KIND_BUTTON, 0, 0, 10, 10, NULL, NULL, NULL);
  ASSERT_TRUE(focus_select("b1"));
  focus_set_pseudo_passive(FOCUS_KIND_LISTBOX);
  ASSERT_EQ_STR(focus_name(), "__listbox__");
  ASSERT_EQ_INT(focus_kind(), FOCUS_KIND_LISTBOX);
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
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
