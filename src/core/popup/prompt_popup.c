#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/context.h"
#include "core/focus.h"
#include "core/prompt_keys.h"
#include "core/prompt_geometry.h"
#include "core/types/prompt.h"
#include "core/types/keys.h"
#include "core/window_manager.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_input_state.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/window.h"

typedef struct {
  bool active;
  long long start_ms;
  int duration_ms;
  int from_x, from_y, from_w, from_h;
  int to_x, to_y, to_w, to_h;
  bool ticking;
} PopupResizeAnim;

typedef struct {
  Popup base;
  PromptSpec spec;
  PromptState *state;
  const PromptCallbacks *saved_callbacks;
  PromptCallbacks state_callbacks;
  WindowHandle prev_hPopupWnd;
  int x, y, w, h;
  PopupResizeAnim resize_anim;
} PromptPopupData;

static void prompt_popup_on_show(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  data->resize_anim.active = false;
  WindowManager *wm = (WindowManager *)self->manager;
  SkaldContext *ctx = SkaldContext_Get();
  ctx->window.selection_complete = false;
  ctx->window.output_buffer[0] = '\0';
  data->prev_hPopupWnd = ctx->window.hPopupWnd;
  input_state_clear_queue();
  log_info("prompt_popup_on_show title=%s x=%d y=%d popup_count=%d",
           data->spec.title ? data->spec.title : "(null)", data->x, data->y,
           wm->popup_count);

  Platform_PromptPopupShowWindow(&(PlatformPromptPopupShowWindow){
      .self = self, .x = data->x, .y = data->y, .w = data->w, .h = data->h});
  Platform_PromptPopupResize(&(PlatformPromptPopupResize){
      .self = self, .x = data->x, .y = data->y, .w = data->w, .h = data->h});
  {
    bool want_rounded =
        data->spec.rounded ? data->spec.rounded : RoundedCorners;
    int want_radius = data->spec.radius > 0 ? data->spec.radius : CornerRadius;
    Platform_SetWindowRounded(self->handle, want_rounded, want_radius);
  }

  data->state = calloc(1, sizeof(PromptState));
  if (!data->state)
    return;
  data->state->eb = calloc(1, sizeof(EditBox));
  if (!data->state->eb) {
    free(data->state);
    data->state = NULL;
    return;
  }

  data->state->eb->cursor = 0;
  data->state->default_eb = data->state->eb;
  data->state->active_editbox = -1;
  data->state->width = data->w;
  data->state->height = data->h;
  const char *title = data->spec.title ? data->spec.title : "Skald";
  data->state->window_title = (char *)title;
  data->state->render_data = data->spec.render_data;
  data->state->eb->filter[0] = '\0';
  {
    SkaldContext *ctx = SkaldContext_Get();
    const char *pending = ctx ? ctx->window.initial_text_pending : NULL;
    if (pending && pending[0]) {
      safe_copy(data->state->eb->filter, sizeof(data->state->eb->filter),
                pending);
      data->state->eb->cursor = (int)strlen(data->state->eb->filter);
    }
    if (ctx)
      ctx->window.initial_text_pending = NULL;
  }
  if (data->spec.items || data->spec.refresh || data->spec.render) {
    data->state->lb = calloc(1, sizeof(ListBox));
  } else {
    data->state->lb = NULL;
  }
  listbox_info_init(data->state->lb);
  data->state->no_bg = data->spec.no_bg;
  data->state->bg_color = data->spec.bg_color;
  data->state->password_mode = data->spec.password_mode;
  int list_h = data->h - prompt_list_top() - prompt_list_bottom_margin();
  data->state->no_separators = data->spec.no_separators ||
                               data->spec.auto_scale ||
                               list_h < LISTBOX_SEPARATOR_MIN_LIST_H;
  if (data->state->lb)
    data->state->lb->selected_index = -1;
  self->user_data = data->state;
  data->state->no_bg = data->spec.no_bg;
  data->state->bg_color = data->spec.bg_color;

  SkaldContext *ctx2 = SkaldContext_Get();
  if (!wm->ctx)
    wm->ctx = ctx2;
  data->saved_callbacks = ctx2->window.prompt_callbacks;
  if (data->saved_callbacks)
    data->state_callbacks = *data->saved_callbacks;
  else
    memset(&data->state_callbacks, 0, sizeof(data->state_callbacks));
  data->state_callbacks.refresh =
      data->spec.refresh ? data->spec.refresh : data->saved_callbacks->refresh;
  data->state_callbacks.enter =
      data->spec.enter ? data->spec.enter : data->saved_callbacks->enter;
  data->state_callbacks.ctrl_enter = data->spec.ctrl_enter
                                         ? data->spec.ctrl_enter
                                         : data->saved_callbacks->ctrl_enter;
  data->state_callbacks.shift_enter = data->spec.shift_enter
                                          ? data->spec.shift_enter
                                          : data->saved_callbacks->shift_enter;
  data->state_callbacks.ctrl_shift_enter =
      data->spec.ctrl_shift_enter ? data->spec.ctrl_shift_enter
                                  : data->saved_callbacks->ctrl_shift_enter;
  data->state_callbacks.ctrl_alt_enter =
      data->spec.ctrl_alt_enter ? data->spec.ctrl_alt_enter
                                : data->saved_callbacks->ctrl_alt_enter;
  data->state_callbacks.shift_alt_enter =
      data->spec.shift_alt_enter ? data->spec.shift_alt_enter
                                 : data->saved_callbacks->shift_alt_enter;
  data->state_callbacks.render =
      data->spec.render ? data->spec.render : data->saved_callbacks->render;
  data->state->callbacks = &data->state_callbacks;
  data->state_callbacks.edit =
      data->spec.edit ? data->spec.edit : data->saved_callbacks->edit;
  data->state_callbacks.filter =
      data->spec.filter ? data->spec.filter : data->saved_callbacks->filter;
  data->state_callbacks.help =
      data->spec.help ? data->spec.help : data->saved_callbacks->help;
  ctx2->window.prompt_callbacks = &data->state_callbacks;
  window_set_current_feature(title);
  ctx2->window.title = title;
  ctx2->window.editbox_placed = true;
  ctx2->window.listbox_placed = false;
  ctx2->window.band_active = false;
  if (data->spec.refresh)
    data->spec.refresh(data->state);
  if (ctx2->window.auto_enter_pending) {
    ctx2->window.auto_enter_pending = false;
    prompt_trigger_enter(data->state, false, false, false);
  }
}

static void prompt_popup_on_hide(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  {
    SkaldContext *_c3 = SkaldContext_Get();
    if (_c3) {
      window_set_current_feature(NULL);
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

static void prompt_popup_render_now(Popup *self) {
  const PromptPopupData *data = (PromptPopupData *)self;
  prompt_popup_tick_resize_animation_popup(self);
  if (!data->state)
    return;
  Platform_PromptPopupRender(self);
}

static void prompt_popup_on_render(Popup *self) {
  const PromptPopupData *data = (PromptPopupData *)self;
  if (data->spec.fps > 0)
    return;
  prompt_popup_render_now(self);
}

static bool prompt_popup_on_key(Popup *self, const PopupKeyEvent *key) {
  log_info("popup on_key: self=%p vk=%d text_len=%d", (void *)self,
           key ? key->vk : -1, key ? key->text_len : -1);
  return Platform_PromptPopupOnKey(self, key);
}

static void prompt_popup_destroy(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  Platform_PromptPopupDestroyWindow(self);
  if (data->state) {
    listbox_info_free(data->state->lb);
    free(data->state->lb);
    for (int i = 0; i < data->state->editbox_count; i++) {
      free(data->state->editboxes[i].eb.placeholder);
      editbox_wrap_cache_free(&data->state->editboxes[i].wrap_cache);
    }
    EditBox *def =
        data->state->default_eb ? data->state->default_eb : data->state->eb;
    free(def->placeholder);
    free(def);
    free(data->state);
  }
  free(data);
}

static const PopupVTable prompt_popup_vtable = {
    .on_show = prompt_popup_on_show,
    .on_hide = prompt_popup_on_hide,
    .on_render = prompt_popup_on_render,
    .on_key = prompt_popup_on_key,
    .destroy = prompt_popup_destroy};

int prompt_popup_get_bounds(PromptPopupGetBounds *argspec) {
  if (!argspec->p || argspec->p->type != POPUP_PROMPT)
    return 0;
  const PromptPopupData *d = (PromptPopupData *)argspec->p;
  if (argspec->x)
    *argspec->x = d->x;
  if (argspec->y)
    *argspec->y = d->y;
  if (argspec->w)
    *argspec->w = d->w;
  if (argspec->h)
    *argspec->h = d->h;
  return 1;
}

static bool prompt_popup_apply_bounds(Popup *p, int x, int y, int w, int h,
                                      bool from_animation) {
  if (w <= 0 || h <= 0)
    return false;
  if (!Platform_PromptPopupResize(&(PlatformPromptPopupResize){
          .self = p, .x = x, .y = y, .w = w, .h = h}))
    return false;
  PromptPopupData *data = (PromptPopupData *)p;
  data->x = x;
  data->y = y;
  data->w = w;
  data->h = h;
  if (data->state) {
    data->state->width = w;
    data->state->height = h;
    int list_h = h - prompt_list_top() - prompt_list_bottom_margin();
    data->state->no_separators = data->spec.no_separators ||
                                 data->spec.auto_scale ||
                                 list_h < LISTBOX_SEPARATOR_MIN_LIST_H;
  }
  Platform_InvalidatePrompt();
  UI_RequestPopupRepaint();
  (void)from_animation;
  return true;
}

bool prompt_popup_resize_current(int x, int y, int w, int h) {
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return false;
  Popup *p = WindowManager_GetTopPopup(wm);
  if (!p || p->type != POPUP_PROMPT)
    return false;
  ((PromptPopupData *)p)->resize_anim.active = false;
  return prompt_popup_apply_bounds(p, x, y, w, h, false);
}

bool prompt_popup_resize_current_animated(
    PromptPopupResizeCurrentAnimated *argspec) {
  if (argspec->w <= 0 || argspec->h <= 0)
    return false;
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return false;
  Popup *p = WindowManager_GetTopPopup(wm);
  if (!p || p->type != POPUP_PROMPT)
    return false;
  PromptPopupData *data = (PromptPopupData *)p;
  PopupResizeAnim *anim = &data->resize_anim;
  anim->active = true;
  anim->start_ms = Platform_GetWallClockMs();
  anim->duration_ms = argspec->duration_ms > 0 ? argspec->duration_ms : 1;
  anim->from_x = data->x;
  anim->from_y = data->y;
  anim->from_w = data->w;
  anim->from_h = data->h;
  anim->to_x = argspec->x;
  anim->to_y = argspec->y;
  anim->to_w = argspec->w;
  anim->to_h = argspec->h;
  prompt_popup_tick_resize_animation_popup(p);
  return true;
}

bool prompt_popup_resize_animating(const Popup *p) {
  if (!p || p->type != POPUP_PROMPT)
    return false;
  const PromptPopupData *data = (const PromptPopupData *)p;
  return data->resize_anim.active;
}

void prompt_popup_tick_resize_animation_popup(Popup *p) {
  if (!p || p->type != POPUP_PROMPT)
    return;
  PromptPopupData *data = (PromptPopupData *)p;
  PopupResizeAnim *anim = &data->resize_anim;
  if (!anim->active || anim->ticking)
    return;
  anim->ticking = true;
  long long elapsed = Platform_GetWallClockMs() - anim->start_ms;
  if (elapsed >= anim->duration_ms) {
    anim->active = false;
    prompt_popup_apply_bounds(p, anim->to_x, anim->to_y, anim->to_w, anim->to_h,
                              true);
  } else {
    float t = (float)elapsed / (float)anim->duration_ms;
    float inv = 1.0f - t;
    float eased = 1.0f - inv * inv * inv;
    int x = anim->from_x + (int)((anim->to_x - anim->from_x) * eased + 0.5f);
    int y = anim->from_y + (int)((anim->to_y - anim->from_y) * eased + 0.5f);
    int w = anim->from_w + (int)((anim->to_w - anim->from_w) * eased + 0.5f);
    int h = anim->from_h + (int)((anim->to_h - anim->from_h) * eased + 0.5f);
    prompt_popup_apply_bounds(p, x, y, w, h, true);
  }
  anim->ticking = false;
}

Popup *prompt_popup_create(WindowManager *wm, const PromptSpec *spec) {
  if (!wm || !spec)
    return NULL;
  focus_clear();

  int scale = (int)Platform_GetUIScale();
  if (scale <= 0)
    scale = 1;
  int layout_w = spec->w > 0 ? spec->w : PromptW * scale;
  int layout_h = spec->h > 0 ? spec->h : PromptH * scale;

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
      layout_w = spec->w > 0 ? spec->w : PromptW;
    if (layout_h <= 0)
      layout_h = spec->h > 0 ? spec->h : PromptH;
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
  bool explicit_x = x != 0;
  bool explicit_y = y != 0;
  if (!explicit_x)
    WindowManager_GetDefaultPromptPosition(layout_w, layout_h, &x, NULL);
  if (!explicit_y) {
    int anchor_h = spec->anchor_y_to_prompt_h ? PromptH : layout_h;
    WindowManager_GetDefaultPromptPosition(layout_w, anchor_h, NULL, &y);
  }
  if (explicit_x || explicit_y) {
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
      if (explicit_x)
        x += wx;
      if (explicit_y)
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

int WindowManager_TopPopupFps(WindowManager *wm) {
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || top->type != POPUP_PROMPT)
    return 0;
  PromptPopupData *data = (PromptPopupData *)top;
  return data->spec.fps;
}

void WindowManager_RenderTick(WindowManager *wm) {
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || top->type != POPUP_PROMPT)
    return;
  PromptPopupData *data = (PromptPopupData *)top;
  if (data->spec.fps <= 0)
    return;
  prompt_popup_render_now(top);
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

void (*prompt_popup_take_on_close(Popup *p))(void) {
  if (!p || p->type != POPUP_PROMPT)
    return NULL;
  PromptPopupData *data = (PromptPopupData *)p;
  void (*cb)(void) = data->spec.on_close;
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
  return ((PromptState *)p->user_data)->lb == (const ListBox *)arg;
}

static bool prompt_handle_matches(Popup *p, void *arg) {
  return WH_EQUALS(p->handle, *(WindowHandle *)arg);
}

PromptState *prompt_popup_find_state(const ListBox *lb) {
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
