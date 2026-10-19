#include "core/flapi.h"
#include <stdio.h>

static bool pixel_color_matches(FlPos pos, FlColor color) {
  FlColor c = fl_get_pixel_color(pos);
  bool ok = c.r == color.r && c.g == color.g && c.b == color.b;
  return ok;
}

static void wait_for_pixel(FlPos pos, FlColor color, double timeout) {
    FlTimer t = fl_start_timer();
    while (!pixel_color_matches(pos, color)) {
        if (fl_end_timer(t) >= timeout) return;
        fl_sleep(0.1);
    }
}

static void pixel_helpers_publish_caps(void) {
  fl_cap_register("pixel.pixel_color_matches", pixel_color_matches);
  fl_cap_register("pixel.wait_for_pixel", wait_for_pixel);
}

FLINT_FEATURE_INIT {
  pixel_helpers_publish_caps();
}

