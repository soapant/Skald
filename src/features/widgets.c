#include <stdio.h>
#include "core/flapi.h"

static void widgets_test_render(FlWidgetState *state) {
    fl_draw_text(.text = "flint widget test", .x = 10, .y = 6, .fontsize = 15,
                 .color = COLOR_TEXT, .align = "LEFT");
}

FLINT_FEATURE_INIT {
    fl_widget(.name = "test",
              .slot = FL_WIDGET_SLOT_TOP_LEFT,
              .render = widgets_test_render);
}
