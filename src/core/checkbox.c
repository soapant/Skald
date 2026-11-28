#include "core/checkbox.h"

#include <string.h>

#include "deps/log.h"
#include "core/focus.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include "config_values.h"
#include "core/context.h"
#include "deps/stb_ds.h"

#define CHECKBOX_BASE 16
#define CHECKBOX_ICON_PATH "resources/check.png"

#define CHECKBOX_DARK_GRAY RGB(14, 14, 14)

static CheckboxSpec **checkboxes;
#define checkbox_count arrlenu(checkboxes)

typedef struct {
  CheckboxSpec *cb;
  unsigned frame;
  long long ms;
} PendingClick;

static WidgetMouseState cb_mouse;
static PendingClick *pending_clicks;
static unsigned cb_frame;
static long long last_click_ms;

static void checkbox_note_click(CheckboxSpec *cb) {
  long long now_ms = Platform_GetWallClockMs();
  long long dt = now_ms - last_click_ms;
  if (dt < 200) {
    log_info("button click ignored debounce name=%s dt=%lld",
             cb->name ? cb->name : "?", dt);
    return;
  }
  last_click_ms = now_ms;
  for (size_t i = 0; i < arrlenu(pending_clicks); i++) {
    if (pending_clicks[i].cb != cb)
      continue;
    pending_clicks[i].frame = cb_frame;
    pending_clicks[i].ms = now_ms;
    log_info("button click noted name=%s frame=%u", cb->name ? cb->name : "?",
             cb_frame);
    return;
  }
  PendingClick *e = arraddnptr(pending_clicks, 1);
  e->cb = cb;
  e->frame = cb_frame;
  e->ms = now_ms;
  log_info("button click noted name=%s frame=%u", cb->name ? cb->name : "?",
           cb_frame);
}

static bool checkbox_take_click(CheckboxSpec *cb) {
  for (size_t i = 0; i < arrlenu(pending_clicks);) {
    if (pending_clicks[i].cb == cb) {
      log_info("button click taken name=%s frame=%u age=%u",
               cb->name ? cb->name : "?", pending_clicks[i].frame,
               cb_frame - pending_clicks[i].frame);
      arrdelswap(pending_clicks, (int)i);
      cb->clicked = false;
      return true;
    }
    i++;
  }
  cb->clicked = false;
  return false;
}

void checkbox_frame_begin(void) {
  cb_frame++;
  cb_mouse.valid = false;
  for (size_t i = 0; i < checkbox_count; i++)
    checkboxes[i]->clicked = false;
  for (size_t i = 0; i < arrlenu(pending_clicks);) {
    if (pending_clicks[i].frame + 1 < cb_frame) {
      log_info("button click expired name=%s frame=%u",
               pending_clicks[i].cb->name ? pending_clicks[i].cb->name : "?",
               pending_clicks[i].frame);
      arrdelswap(pending_clicks, (int)i);
    } else {
      i++;
    }
  }
}

static void checkbox_focus_activate(void *owner) {
  CheckboxSpec *cb = (CheckboxSpec *)owner;
  if (!cb)
    return;
  cb->clicked = true;
  checkbox_note_click(cb);
  if (!cb->button)
    cb->checked = !cb->checked;
  if (cb->on_click) {
    cb->on_click();
    checkbox_take_click(cb);
  }
}

static CheckboxSpec *checkbox_find(const char *name, int kind) {

  if (!name)
    return NULL;
  for (size_t i = 0; i < checkbox_count; i++) {
    if (strcmp(checkboxes[i]->name, name) == 0 &&
        checkboxes[i]->button == (kind == PLACE_BUTTON))
      return checkboxes[i];
  }
  return NULL;
}

static CheckboxSpec *checkbox_intern(const char *name, int kind, bool seed) {

  CheckboxSpec *cb = checkbox_find(name, kind);
  if (cb)
    return cb;
  cb = (CheckboxSpec *)calloc(1, sizeof(*cb));
  if (!cb)
    return NULL;
  cb->button = (kind == PLACE_BUTTON);
  cb->checkbox = (kind == PLACE_CHECKBOX);
  cb->checked = seed;
  cb->thickness = 1;
  cb->border = true;
  cb->name = strdup(name);
  if (!cb->name) {
    free(cb);
    return NULL;
  }
  arrput(checkboxes, cb);
  return cb;
}

bool checkbox_is_checked(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->checked : false;
}

bool checkbox_is_hovered(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->hovered : false;
}

bool checkbox_is_clicked(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->clicked : false;
}

bool checkbox_take_clicked(const char *name, int kind) {
  CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? checkbox_take_click(cb) : false;
}

static void checkbox_fill_border(int x, int y, int w, int h, int thick,
                                 int body_radius, bool body_round,
                                 int border_radius, unsigned long body,
                                 unsigned long border_col, bool with_border) {
  body = rgb_to_hex(body);
  border_col = rgb_to_hex(border_col);
  Platform_DrawRectRaw((DrawRectRawSpec){.x = x,
                                         .y = y,
                                         .w = w,
                                         .h = h,
                                         .color = body,
                                         .radius = body_radius,
                                         .rounded = body_round,
                                         .opaque = true,
                                         .thickness = thick,
                                         .filled = true});
  if (!with_border)
    return;
  Platform_DrawRectRaw((DrawRectRawSpec){.x = x,
                                         .y = y,
                                         .w = w,
                                         .h = h,
                                         .color = border_col,
                                         .radius = border_radius,
                                         .rounded = true,
                                         .opaque = true,
                                         .thickness = thick,
                                         .filled = false});
}

static void checkbox_focus_outline(const CheckboxSpec *spec) {
  if (!spec || !spec->focused)
    return;
  int w = spec->w > 0 ? spec->w : (spec->button ? 100 : 16);
  int h = spec->h > 0 ? spec->h : (spec->button ? 24 : 16);
  Platform_DrawRectRaw((DrawRectRawSpec){.x = spec->x - 3,
                                         .y = spec->y - 3,
                                         .w = w + 6,
                                         .h = h + 6,
                                         .color = rgb_to_hex(widget_accent()),
                                         .radius = CornerRadius / 1.5,
                                         .rounded = true,
                                         .opaque = true,
                                         .thickness = 2,
                                         .filled = false});
}

void checkbox_draw(const CheckboxSpec *spec) {
  bool active = spec->hovered || spec->focused;
  if (spec->render) {
    PlaceState st;
    memset(&st, 0, sizeof(st));
    st.rect.x = spec->x;
    st.rect.y = spec->y;
    st.rect.w = spec->w > 0 ? spec->w : (spec->button ? 100 : 16);
    st.rect.h = spec->h > 0 ? spec->h : (spec->button ? 24 : 16);
    st.name = spec->name;
    st.kind = spec->button ? PLACE_BUTTON : PLACE_CHECKBOX;
    st.hovered = active;
    st.checked = spec->checked;
    st.clicked = spec->clicked;
    spec->render(st);
    checkbox_focus_outline(spec);
    return;
  }
  unsigned long def_cb_border = active ? widget_accent() : widget_neutral();
  unsigned long def_btn_border =
      (spec->clicked || active) ? widget_accent() : widget_neutral();

  unsigned long def_cb_body =
      active ? widget_neutral() : (spec->bg_color ? spec->bg_color : COLOR_BG);
  unsigned long def_btn_body = (spec->clicked || active) ? widget_neutral()
                               : spec->bg_color          ? spec->bg_color
                                                         : COLOR_BG;

  unsigned long checkbox_border =
      spec->hovered_color ? spec->hovered_color : def_cb_border;
  unsigned long checkbox_body =
      spec->hovered_color ? spec->hovered_color : def_cb_body;

  unsigned long button_border =
      spec->clicked_color ? spec->clicked_color : def_btn_border;
  unsigned long button_body =
      spec->clicked_color ? spec->clicked_color : def_btn_body;

  int thick = spec->thickness > 0 ? spec->thickness : 1;

  if (spec->checkbox) {
    int w = spec->w > 0 ? spec->w : 16;
    int h = spec->h > 0 ? spec->h : 16;
    if (!spec->no_bg) {
      checkbox_fill_border(spec->x, spec->y, w, h, thick, CornerRadius / 1.5,
                           true, CornerRadius / 1.5, checkbox_body,
                           checkbox_border, spec->border);
    }

    if (spec->checked) {
      int m = w < h ? w : h;
      int pad = m / 8;
      int s = m - 2 * pad;
      Platform_DrawImageAt(&(PlatformDrawImageAt){.f_path = CHECKBOX_ICON_PATH, .f_x = spec->x + (w - s) / 2, .f_y = spec->y + (h - s) / 2, .f_w = s, .f_h = s});
    }
  } else if (spec->button) {
    int w = spec->w > 0 ? spec->w : 100;
    int h = spec->h > 0 ? spec->h : 24;
    if (!spec->no_bg) {
      checkbox_fill_border(spec->x, spec->y, w, h, thick, CornerRadius / 1.2,
                           true, CornerRadius / 1.2, button_body, button_border,
                           spec->border);
    }
    if (spec->text) {
      int font_size = spec->font_size > 0 ? spec->font_size : FontSize;
      Platform_DrawTextCentered((DrawTextCenteredSpec){
          .x = spec->x,
          .y = spec->y,
          .w = w,
          .h = h,
          .text = spec->text,
          .color = spec->text_color ? rgb_to_hex(spec->text_color)
                                    : rgb_to_hex(widget_accent()),
          .font_size = font_size});
    }
  }
  checkbox_focus_outline(spec);
}

bool checkbox_place(const CheckboxSpec *spec) {
  if (!spec || !spec->name || !spec->name[0]) {
    return false;
  }

  int w, h;
  int x = spec->x, y = spec->y;

  if (spec->button) {
    w = spec->w > 0 ? spec->w : 100;
    h = spec->h > 0 ? spec->h : 24;
  } else {
    w = spec->w > 0 ? spec->w : CHECKBOX_BASE;
    h = spec->h > 0 ? spec->h : CHECKBOX_BASE;
  }

  CheckboxSpec *cb = checkbox_intern(
      spec->name, spec->button ? PLACE_BUTTON : PLACE_CHECKBOX, spec->checked);
  if (!cb)
    return false;
  cb->on_click = spec->on_click;
  cb->on_hover = spec->on_hover;
  cb->checkbox = spec->checkbox;
  cb->button = spec->button;
  cb->thickness = spec->thickness;
  cb->hovered_color = spec->hovered_color;
  cb->clicked_color = spec->clicked_color;
  cb->border = spec->border;
  cb->no_bg = spec->no_bg;
  cb->w = w;
  cb->h = h;
  cb->x = spec->x;
  cb->y = spec->y;
  cb->text = spec->text;
  cb->font_size = spec->font_size;
  cb->text_color = spec->text_color;
  cb->render = spec->render;

  focus_register(&(FocusRegister){.f_name = spec->name, .f_kind = spec->button ? FOCUS_KIND_BUTTON : FOCUS_KIND_CHECKBOX, .f_x = x, .f_y = y, .f_w = w, .f_h = h, .f_owner = cb, .f_activate = checkbox_focus_activate, .f_adjust = NULL});
  cb->focused = focus_is(spec->name);

  int mx = 0, my = 0;
  bool down = false;
  widget_mouse_poll(&cb_mouse, &mx, &my, &down);

  bool hovered = (mx >= x && mx < x + w && my >= y && my < y + h);
  bool hover_changed = (hovered != cb->hovered);
  cb->hovered = hovered;

  bool clicked = false;

  if (down && !cb->was_down) {
    if (hovered) {
      cb->checked = !cb->checked;
      cb->clicked = true;
      checkbox_note_click(cb);
      clicked = true;
      focus_select(spec->name);
    }
  }
  cb->was_down = down;

  checkbox_draw(cb);

  if (clicked && cb->on_click) {
    cb->on_click();
    checkbox_take_click(cb);
  }
  if (hover_changed && cb->on_hover)
    cb->on_hover();
  return true;
}

