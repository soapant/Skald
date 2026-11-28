#if defined(__linux__)

#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "wm/wm.h"

TEST(snap_halves_and_quarters) {
  SnapRect r = wm_snap_rect(&(FnWmSnapRectSpec){.f_mx = 10, .f_my = 20, .f_mw = 800, .f_mh = 600, .f_p = SNAP_LEFT});
  ASSERT_EQ_INT(r.x, 10);
  ASSERT_EQ_INT(r.y, 20);
  ASSERT_EQ_INT(r.w, 400);
  ASSERT_EQ_INT(r.h, 600);

  r = wm_snap_rect(&(FnWmSnapRectSpec){.f_mx = 10, .f_my = 20, .f_mw = 800, .f_mh = 600, .f_p = SNAP_RIGHT});
  ASSERT_EQ_INT(r.x, 410);
  ASSERT_EQ_INT(r.w, 400);

  r = wm_snap_rect(&(FnWmSnapRectSpec){.f_mx = 10, .f_my = 20, .f_mw = 800, .f_mh = 600, .f_p = SNAP_TOP});
  ASSERT_EQ_INT(r.y, 20);
  ASSERT_EQ_INT(r.h, 300);

  r = wm_snap_rect(&(FnWmSnapRectSpec){.f_mx = 10, .f_my = 20, .f_mw = 800, .f_mh = 600, .f_p = SNAP_BOTTOM_RIGHT});
  ASSERT_EQ_INT(r.x, 410);
  ASSERT_EQ_INT(r.y, 320);
  ASSERT_EQ_INT(r.w, 400);
  ASSERT_EQ_INT(r.h, 300);
}

TEST(json_num_at_parses_and_rejects) {
  long v = -1;
  ASSERT_TRUE(wm_json_num_at("{\"id\":42,\"x\":7}", "\"id\"", &v));
  ASSERT_EQ_INT(v, 42);
  ASSERT_FALSE(wm_json_num_at("{\"id\":42}", "\"missing\"", &v));
  ASSERT_FALSE(wm_json_num_at(NULL, "\"id\"", &v));
  ASSERT_FALSE(wm_json_num_at("{\"id\":abc}", "\"id\"", &v));
}

TEST(trim_focus_title_handles_whitespace_and_null) {
  ASSERT_TRUE(wm_trim_focus_title(NULL) == NULL);
  ASSERT_EQ_STR(wm_trim_focus_title("  \tHello World\n\r "), "Hello World");
  ASSERT_EQ_STR(wm_trim_focus_title(""), "");
  ASSERT_EQ_STR(wm_trim_focus_title("no padding"), "no padding");
}

TEST(append_window_name_filters_placeholder_titles) {
  WindowList list = {0};
  ASSERT_TRUE(wm_append_window_name(&list, "window", false));
  ASSERT_TRUE(wm_append_window_name(&list, "COLORED", true));
  ASSERT_EQ_INT(list.count, 0);

  ASSERT_TRUE(wm_append_window_name(&list, "firefox", true));
  ASSERT_EQ_INT(list.count, 1);
  ASSERT_EQ_STR(list.windows[0], "Firefox");

  for (int i = 0; i < list.count; i++)
    free(list.windows[i]);
  free(list.windows);
}

int main(void) {
  RUN(snap_halves_and_quarters);
  RUN(json_num_at_parses_and_rejects);
  RUN(trim_focus_title_handles_whitespace_and_null);
  RUN(append_window_name_filters_placeholder_titles);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}

#else

int main(void) { return 0; }

#endif
