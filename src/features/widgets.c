#include <stdio.h>
#include "core/flapi.h"

static void widgets_test_render(FlWidgetState *state) {
    char line[128];
    snprintf(line, sizeof(line), "frame %lu   %.1fs elapsed", state->frame, state->elapsed);

    fl_draw_text(.text = "flint widget test", .x = 10, .y = 6, .fontsize = 15,
                 .color = COLOR_TEXT, .align = "LEFT");
}

FLINT_FEATURE_INIT {
    fl_widget(.name = "test",
              .w = 160,
              .h = 46,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_test_render);
}
