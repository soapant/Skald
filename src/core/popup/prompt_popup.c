#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/context.h"
#include "core/types/prompt.h"
#include "core/types/keys.h"
#include "core/window_manager.h"
#include "helpers/helpers_text.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_input_state.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/window.h"

typedef struct {
  Popup base;
  PromptSpec spec;
  PromptState *state;
  PromptCallbacks saved_callbacks;
  WindowHandle prev_hPopupWnd;
  int x, y, w, h;
} PromptPopupData;

static void prompt_popup_on_show(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  WindowManager *wm = (WindowManager *)self->manager;
  SkaldContext *ctx = SkaldContext_Get();
  ctx->window.selection_complete = false;
  ctx->window.output_buffer[0] = '\0';
  data->prev_hPopupWnd = ctx->window.hPopupWnd;
  input_state_clear_queue();
  log_info("prompt_popup_on_show title=%s x=%d y=%d popup_count=%d",
           data->spec.title ? data->spec.title : "(null)", data->x, data->y,
           wm->popup_count);

  Platform_PromptPopupShowWindow(self, data->x, data->y, data->w, data->h);

  data->state = calloc(1, sizeof(PromptState));
  if (!data->state)
    return;
  data->state->eb = calloc(1, sizeof(EditBoxInfo));
  if (!data->state->eb) {
    free(data->state);
    data->state = NULL;
    return;
  }

  data->state->eb->cursor = 0;
  data->state->width = data->w;
  data->state->height = data->h;
  const char *title = data->spec.title ? data->spec.title : "Skald";
  data->state->window_title = (char *)title;
  data->state->eb->filter[0] = '\0';
  if (data->spec.initial_text && data->spec.initial_text[0]) {
    safe_copy(data->state->eb->filter, sizeof(data->state->eb->filter),
             data->spec.initial_text);
    data->state->eb->cursor = (int)strlen(data->state->eb->filter);
  }
  if (data->spec.password_mode &&
      (!data->spec.placeholder || !data->spec.placeholder[0])) {
    data->state->eb->placeholder = strdup("Enter Password:");
  } else if (data->spec.placeholder && data->spec.placeholder[0]) {
    data->state->eb->placeholder = strdup(data->spec.placeholder);
  }
  if (data->spec.items || data->spec.refresh || data->spec.render) {
    data->state->lb = calloc(1, sizeof(ListBoxInfo));
  } else {
    data->state->lb = NULL;
  }
  listbox_info_init(data->state->lb);
  data->state->no_bg = data->spec.no_bg;
  data->state->bg_color = data->spec.bg_color;
  data->state->password_mode = data->spec.password_mode;
  if (data->state->lb)
    data->state->lb->selected_index = -1;
  self->user_data = data->state;
  data->state->no_bg = data->spec.no_bg;
  data->state->bg_color = data->spec.bg_color;

  SkaldContext *ctx2 = SkaldContext_Get();
  if (!wm->ctx)
    wm->ctx = ctx2;
  log_info("BEFORE SAVE: ctx=%p enter=%p ctrl_enter=%p", (void *)ctx2,
           (void *)ctx2->window.prompt_callbacks.enter,
           (void *)ctx2->window.prompt_callbacks.ctrl_enter);
  data->saved_callbacks = ctx2->window.prompt_callbacks;
  log_info("DEBUG_SAVE_CALLBACKS title='%s' saved_enter=%p saved_ctrl_enter=%p "
           "spec_enter=%p spec_ctrl_enter=%p",
           data->spec.title ? data->spec.title : "(null)",
           (void *)data->saved_callbacks.enter,
           (void *)data->saved_callbacks.ctrl_enter, (void *)data->spec.enter,
           (void *)data->spec.ctrl_enter);
  ctx2->window.prompt_callbacks.refresh =
      data->spec.refresh ? data->spec.refresh : data->saved_callbacks.refresh;
  ctx2->window.prompt_callbacks.enter =
      data->spec.enter ? data->spec.enter : data->saved_callbacks.enter;
  ctx2->window.prompt_callbacks.ctrl_enter =
      data->spec.ctrl_enter ? data->spec.ctrl_enter
                            : data->saved_callbacks.ctrl_enter;
  ctx2->window.prompt_callbacks.shift_enter =
      data->spec.shift_enter ? data->spec.shift_enter
                             : data->saved_callbacks.shift_enter;
  ctx2->window.prompt_callbacks.ctrl_shift_enter =
      data->spec.ctrl_shift_enter ? data->spec.ctrl_shift_enter
                                  : data->saved_callbacks.ctrl_shift_enter;
  ctx2->window.prompt_callbacks.ctrl_alt_enter =
      data->spec.ctrl_alt_enter ? data->spec.ctrl_alt_enter
                                : data->saved_callbacks.ctrl_alt_enter;
  ctx2->window.prompt_callbacks.shift_alt_enter =
      data->spec.shift_alt_enter ? data->spec.shift_alt_enter
                                 : data->saved_callbacks.shift_alt_enter;
  ctx2->window.prompt_callbacks.tab =
      data->spec.tab ? data->spec.tab : data->saved_callbacks.tab;
  ctx2->window.prompt_callbacks.render =
      data->spec.render ? data->spec.render : data->saved_callbacks.render;
  data->state->callbacks = &SkaldContext_Get()->window.prompt_callbacks;
  ctx2->window.prompt_callbacks.edit =
      data->spec.edit ? data->spec.edit : data->saved_callbacks.edit;
  ctx2->window.prompt_callbacks.filter =
      data->spec.filter ? data->spec.filter : data->saved_callbacks.filter;
  ctx2->window.prompt_callbacks.help =
      data->spec.help ? data->spec.help : data->saved_callbacks.help;
  ctx2->window.current_feature = (char *)title;
  ctx2->window.title = title;
  ctx2->window.editbox_placed = true;
  if (data->spec.refresh)
    data->spec.refresh(data->state);
}

static void prompt_popup_on_hide(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  log_info("DEBUG_ON_HIDE title='%s'",
           data->spec.title ? data->spec.title : "(null)");
  {
    SkaldContext *_c3 = SkaldContext_Get();
    if (_c3) {
      _c3->window.current_feature = NULL;
      _c3->window.title = NULL;
      _c3->window.prompt_callbacks = data->saved_callbacks;
      _c3->window.hPopupWnd = data->prev_hPopupWnd;
    }
  }
  if (data->state) {
    if (data->state->lb)
      data->state->lb->selected_index = 0;
  }
  Platform_PromptPopupHideWindow(self);
}

static void prompt_popup_on_render(Popup *self) {
  const PromptPopupData *data = (PromptPopupData *)self;
  if (!data->state)
    return;
  Platform_PromptPopupRender(self);
}

static bool prompt_popup_on_key(Popup *self, const PopupKeyEvent *key) {
  return Platform_PromptPopupOnKey(self, key);
}

static void prompt_popup_destroy(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  Platform_PromptPopupDestroyWindow(self);
  if (data->state) {
    listbox_info_free(data->state->lb);
    free(data->state->eb->placeholder);
    free(data->state->eb);
    free(data->state);
  }
  free(data);
}

static const PopupVTable prompt_popup_vtable = {.on_show = prompt_popup_on_show,
                                                .on_hide = prompt_popup_on_hide,
                                                .on_render =
                                                    prompt_popup_on_render,
                                                .on_key = prompt_popup_on_key,
                                                .destroy = prompt_popup_destroy};

int prompt_popup_get_bounds(Popup *p, int *x, int *y, int *w, int *h) {
  if (!p || p->type != POPUP_PROMPT)
    return 0;
  const PromptPopupData *d = (PromptPopupData *)p;
  if (x)
    *x = d->x;
  if (y)
    *y = d->y;
  if (w)
    *w = d->w;
  if (h)
    *h = d->h;
  return 1;
}

Popup *prompt_popup_create(WindowManager *wm, const PromptSpec *spec) {
  if (!wm || !spec)
    return NULL;

  int scale = (int)Platform_GetUIScale();
  if (scale <= 0)
    scale = 1;
  int layout_w = spec->w > 0 ? spec->w : PromptWidth * scale;
  int layout_h = spec->h > 0 ? spec->h : PromptHeight * scale;

  {
    int mx = 0, my = 0, mw = 0, mh = 0;
    WindowManager_GetMainMonitorGeometry(&mx, &my, &mw, &mh);
    if (mw > 0) {
      if (layout_w > mw - 40)
        layout_w = mw - 40;
      if (layout_w < 1)
        layout_w = mw > 140 ? mw - 40 : mw;
    }
    if (mh > 0) {
      if (layout_h > mh - 40)
        layout_h = mh - 40;
      if (layout_h < 1)
        layout_h = mh > 140 ? mh - 40 : mh;
    }
    if (layout_w <= 0)
      layout_w = spec->w > 0 ? spec->w : PromptWidth;
    if (layout_h <= 0)
      layout_h = spec->h > 0 ? spec->h : PromptHeight;
  }

  if (spec->auto_scale) {
    int rows = 0;
    if (spec->items) {
      while (spec->items[rows])
        rows++;
    }
    if (rows <= 0)
      rows = 1;
    int line_h = 0, row_h = 0, list_top = 0;
    Platform_GetTextLayoutMetrics(&line_h, &row_h, &list_top);
    if (row_h <= 0)
      row_h = 16 * scale;
    int edit_h = row_h * 2;
    if (edit_h < 40)
      edit_h = 40;
    int desired_h = edit_h + rows * row_h + 20;
    int mx, my, mw, mh;
    WindowManager_GetMainMonitorGeometry(&mx, &my, &mw, &mh);
    int max_h = mh - 40;
    if (max_h < 100)
      max_h = 100;
    if (desired_h > max_h)
      desired_h = max_h;
    if (desired_h < edit_h + row_h)
      desired_h = edit_h + row_h;
    layout_h = desired_h;
    if (layout_w > mw - 40)
      layout_w = mw - 40;
  }

  int x = spec->x;
  int y = spec->y;
  if (x == 0 && y == 0) {
    WindowManager_GetDefaultPromptPosition(layout_w, layout_h, &x, &y);
  } else {
    int wx = 0, wy = 0;
    Popup *top = WindowManager_GetTopPopup(wm);
    bool have_origin = false;
    if (top)
      have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
    if (!have_origin) {
      WindowHandle hMain = SkaldContext_Get()->window.hPopupWnd;
      have_origin = Platform_WindowToRoot(hMain, &wx, &wy);
    }
    if (have_origin) {
      x += wx;
      y += wy;
    }
  }

  PromptPopupData *data = calloc(1, sizeof(PromptPopupData));
  if (!data)
    return NULL;

  data->base.vtable = &prompt_popup_vtable;
  data->base.manager = wm;
  data->base.type = POPUP_PROMPT;
  data->spec = *spec;
  data->x = x;
  data->y = y;
  data->w = layout_w;
  data->h = layout_h;
  return WindowManager_PushPopup(wm, (Popup *)data);
}

bool WindowManager_TopPopupWantsTick(WindowManager *wm) {
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || top->type != POPUP_PROMPT)
    return false;
  PromptPopupData *data = (PromptPopupData *)top;
  return data->spec.tick;
}

bool WindowManager_IsPointInTopPopup(WindowManager *wm, int root_x,
                                     int root_y) {
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || top->type != POPUP_PROMPT)
    return true;
  const PromptPopupData *data = (PromptPopupData *)top;
  return (root_x >= data->x && root_x <= data->x + data->w &&
          root_y >= data->y && root_y <= data->y + data->h);
}

CloseCallback PromptPopup_TakeOnClose(Popup *p) {
  if (!p || p->type != POPUP_PROMPT)
    return NULL;
  PromptPopupData *data = (PromptPopupData *)p;
  CloseCallback cb = data->spec.on_close;
  data->spec.on_close = NULL;
  return cb;
}

static Popup *prompt_find_popup(bool (*match)(Popup *p, void *arg), void *arg) {
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  for (int i = wm->popup_count - 1; i >= 0; i--) {
    Popup *p = wm->popups[i];
    if (!p || p->type != POPUP_PROMPT || !p->user_data)
      continue;
    if (match(p, arg))
      return p;
  }
  return NULL;
}

static bool prompt_lb_matches(Popup *p, void *arg) {
  return ((PromptState *)p->user_data)->lb == (const ListBoxInfo *)arg;
}

static bool prompt_handle_matches(Popup *p, void *arg) {
  return WH_EQUALS(p->handle, *(WindowHandle *)arg);
}

PromptState *prompt_popup_find_state(const ListBoxInfo *lb) {
  Popup *p = prompt_find_popup(prompt_lb_matches, (void *)lb);
  return p ? (PromptState *)p->user_data : NULL;
}

Popup *prompt_popup_find_popup_by_handle(WindowHandle handle) {
  if (WH_EQUALS(handle, WH_NULL))
    return NULL;
  return prompt_find_popup(prompt_handle_matches, &handle);
}

PromptState *prompt_popup_get_top_prompt_state(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->type == POPUP_PROMPT && top->user_data)
    return (PromptState *)top->user_data;
  return NULL;
}
