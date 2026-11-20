#include "test.h"
#include "core/skapi.h"
#include "core/scroll_area.h"
#include "helpers/helpers_input_state.h"

TEST(scroll_event_queue_roundtrip) {
  sk_clear_scroll_queue();
  ScrollEvent a = {0};
  a.x = 10;
  a.y = 20;
  a.dy = -1;
  a.mods = 3;
  global_scroll_push(&a);
  ScrollEvent b = {0};
  b.x = 30;
  b.y = 40;
  b.dx = 2;
  global_scroll_push(&b);

  ASSERT_EQ_INT(sk_scroll_queue_len(), 2);
  SkScrollEvent ev = {0};
  ASSERT_TRUE(sk_poll_scroll(&ev));
  ASSERT_EQ_INT(ev.x, 10);
  ASSERT_EQ_INT(ev.y, 20);
  ASSERT_EQ_INT(ev.dy, -1);
  ASSERT_EQ_INT(ev.mods, 3);
  ASSERT_TRUE(sk_poll_scroll(&ev));
  ASSERT_EQ_INT(ev.dx, 2);
  ASSERT_FALSE(sk_poll_scroll(&ev));
}

TEST(scroll_area_wheel_clamps_offset) {
  ski_set_draw_offset(0, 0);
  int off_y = 0;
  SkScrollSpec spec = {
      .name = "test-scroll-area",
      .x = 0,
      .y = 0,
      .w = 100,
      .h = 100,
      .content_w = 0,
      .content_h = 500,
      .off_y = &off_y,
      .wheel_px = 48,
  };
  ASSERT_TRUE(ski_scroll_begin(&spec));
  ASSERT_EQ_INT(off_y, 0);
  ASSERT_TRUE(scroll_area_dispatch_wheel(10, 10, 0, 1));
  ASSERT_EQ_INT(off_y, 48);
  ASSERT_TRUE(scroll_area_dispatch_wheel(10, 10, 0, 100));
  ASSERT_EQ_INT(off_y, 400);
  ASSERT_TRUE(scroll_area_dispatch_wheel(10, 10, 0, -100));
  ASSERT_EQ_INT(off_y, 0);
  ASSERT_FALSE(scroll_area_dispatch_wheel(500, 500, 0, 1));
  sk_scroll_end();
  int dx = -1, dy = -1;
  ski_get_draw_offset(&dx, &dy);
  ASSERT_EQ_INT(dx, 0);
  ASSERT_EQ_INT(dy, 0);
}

int main(void) {
  RUN(scroll_event_queue_roundtrip);
  RUN(scroll_area_wheel_clamps_offset);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
