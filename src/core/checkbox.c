#include "core/checkbox.h"

#include <string.h>

#include "deps/log.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include "config_values.h"
#include "core/context.h"

#define CHECKBOX_MAX 1024
#define CHECKBOX_BASE 16
#define CHECKBOX_ICON_PATH "resources/check.png"

#define CHECKBOX_WHITE RGB(155, 155, 155)
#define CHECKBOX_GRAY RGB(68, 68, 68)
#define CHECKBOX_DARK_GRAY RGB(14, 14, 14)

static CheckboxSpec checkboxes[CHECKBOX_MAX];
static int checkbox_count = 0;

static struct {
  bool valid;
  int mx, my;
  bool down;
} cb_mouse;

void Checkbox_FrameBegin(void) { cb_mouse.valid = false; }

static void checkbox_mouse(int *out_x, int *out_y, bool *out_down) {

  if (!cb_mouse.valid) {
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      int wx = 0, wy = 0;
      WindowManager *wm = WindowManager_Get();
      Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
      bool have_origin = false;
      if (top)
        have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
      if (!have_origin) {
        have_origin = Platform_WindowToRoot(
            SkaldContext_Get()->window.hPopupWnd, &wx, &wy);
      }
      if (have_origin) {
        mx -= wx;
        my -= wy;
      }
      cb_mouse.mx = mx;
      cb_mouse.my = my;
    } else {
      cb_mouse.mx = -9999;
      cb_mouse.my = -9999;
    }
    cb_mouse.down = Input_MouseStateDown(MOUSE_BTN_LEFT);
    cb_mouse.valid = true;
  }
  if (out_x)
    *out_x = cb_mouse.mx;
  if (out_y)
    *out_y = cb_mouse.my;
  if (out_down)
    *out_down = cb_mouse.down;
}

static CheckboxSpec *checkbox_find(const char *name, int kind) {

  if (!name)
    return NULL;
  for (int i = 0; i < checkbox_count; i++) {
    if (strcmp(checkboxes[i].name, name) == 0 &&
        checkboxes[i].button == (kind == PLACE_BUTTON))
      return &checkboxes[i];
  }
  return NULL;
}

static CheckboxSpec *checkbox_intern(const char *name, int kind, bool seed) {

  CheckboxSpec *cb = checkbox_find(name, kind);
  if (cb)
    return cb;
  if (checkbox_count >= CHECKBOX_MAX) {
    log_error("checkbox table full (%d), refusing '%s'", CHECKBOX_MAX, name);
    return NULL;
  }
  cb = &checkboxes[checkbox_count++];
  memset(cb, 0, sizeof(*cb));
  cb->button = (kind == PLACE_BUTTON);
  cb->checkbox = (kind == PLACE_CHECKBOX);
  cb->checked = seed;
  cb->thickness = 1;
  cb->border = true;
  cb->name = strdup(name);
  if (!cb->name) {
    checkbox_count--;
    return NULL;
  }
  return cb;
}

bool Checkbox_IsChecked(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->checked : false;
}

bool Checkbox_IsHovered(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->hovered : false;
}

bool Checkbox_IsClicked(const char *name, int kind) {
  const CheckboxSpec *cb = name ? checkbox_find(name, kind) : NULL;
  return cb ? cb->clicked : false;
}

void Checkbox_ResetAll(void) {
  for (int i = 0; i < checkbox_count; i++) {
    checkboxes[i].checked = false;
    checkboxes[i].clicked = false;
    checkboxes[i].hovered = false;
    checkboxes[i].was_down = false;
    checkboxes[i].on_click = NULL;
    checkboxes[i].on_hover = NULL;
    checkboxes[i].render = NULL;
  }
}

static void checkbox_fill_border(int x, int y, int w, int h, int thick,
                                 int body_radius, bool body_round,
                                 int border_radius, unsigned long body,
                                 unsigned long border_col, bool with_border) {
  Platform_DrawRectRaw((PlDrawRectRawSpec){.x = x,
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
  Platform_DrawRectRaw((PlDrawRectRawSpec){.x = x,
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

FontHandle s_font;

void Checkbox_Draw(const CheckboxSpec *spec) {
  if (spec->render) {
    PlaceState st;
    memset(&st, 0, sizeof(st));
    st.rect.x = spec->x;
    st.rect.y = spec->y;
    st.rect.w = spec->w > 0 ? spec->w : (spec->button ? 100 : 16);
    st.rect.h = spec->h > 0 ? spec->h : (spec->button ? 24 : 16);
    st.name = spec->name;
    st.kind = spec->button ? PLACE_BUTTON : PLACE_CHECKBOX;
    st.hovered = spec->hovered;
    st.checked = spec->checked;
    st.clicked = spec->clicked;
    spec->render(st);
    return;
  }
  unsigned long def_cb_border = spec->hovered ? CHECKBOX_WHITE : CHECKBOX_GRAY;
  unsigned long def_btn_border =
      (spec->clicked || spec->hovered) ? CHECKBOX_WHITE : CHECKBOX_GRAY;

  unsigned long def_cb_body =
      spec->hovered ? CHECKBOX_GRAY
                    : (spec->bg_color ? spec->bg_color : COLOR_BG);
  unsigned long def_btn_body = (spec->clicked || spec->hovered) ? CHECKBOX_GRAY
                               : spec->bg_color                 ? spec->bg_color
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
      checkbox_fill_border(spec->x, spec->y, w, h, thick, 0, false,
                           CornerRadius / 3, checkbox_body, checkbox_border,
                           spec->border);
    }

    if (spec->checked) {
      int m = w < h ? w : h;
      int pad = m / 8;
      int s = m - 2 * pad;
      Platform_DrawImageAt(CHECKBOX_ICON_PATH, spec->x + (w - s) / 2,
                           spec->y + (h - s) / 2, s, s);
    }
  } else if (spec->button) {
    int w = spec->w > 0 ? spec->w : 100;
    int h = spec->h > 0 ? spec->h : 24;
    if (!spec->no_bg) {
      checkbox_fill_border(spec->x, spec->y, w, h, thick, CornerRadius / 2,
                           true, CornerRadius / 2, button_body, button_border,
                           spec->border);
    }
    if (spec->text) {
      int fontsize = spec->fontsize > 0 ? spec->fontsize : FontSize;
      if (!Platform_FontInitialized(s_font))
        s_font = Platform_CreateFont(fontsize, UsedFont);
      Platform_DrawTextCentered((PlDrawTextCenteredSpec){
          .x = spec->x,
          .y = spec->y,
          .w = w,
          .h = h,
          .text = spec->text,
          .color =
              spec->text_color ? RgbToHex(spec->text_color) : CHECKBOX_WHITE,
          .font = s_font,
          .fontsize = fontsize});
    }
  }
}

bool Checkbox_Place(const CheckboxSpec *spec) {
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
  cb->fontsize = spec->fontsize;
  cb->text_color = spec->text_color;
  cb->render = spec->render;

  int mx = 0, my = 0;
  bool down = false;
  checkbox_mouse(&mx, &my, &down);

  bool hovered = (mx >= x && mx < x + w && my >= y && my < y + h);
  bool hover_changed = (hovered != cb->hovered);
  cb->hovered = hovered;

  bool clicked = false;

  if (cb->button) {
    cb->clicked = false;
  }

  if (down && !cb->was_down) {
    if (hovered) {
      cb->checked = !cb->checked;
      cb->clicked = true;
      clicked = true;
    }
  }
  cb->was_down = down;

  Checkbox_Draw(cb);

  if (clicked && cb->on_click)
    cb->on_click();
  if (hover_changed && cb->on_hover)
    cb->on_hover();
  return true;
}
