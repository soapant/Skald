#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "core/history.h"
#include "platform/wm_platform.h"
#include "deps/log.h"
#include "core/types.h"
#include "core/types/window.h"

typedef struct {
  Popup base;
  PromptSpec spec;
  PromptState *state;
  PromptCallbacks saved_callbacks;
  WindowHandle prev_hPopupWnd;
  int x, y, w, h;
} PromptPopupData;

static const char *prompt_feature_name(const char *title) {
  if (!title || !title[0])
    return title;
  const char *p = strchr(title, ' ');
  if (!p)
    return title;
  for (const char *q = title; q < p; q++) {
    if ((unsigned char)*q >= 0x80)
      return p + 1;
  }
  return title;
}

static void PromptPopup_OnShow(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  WindowManager *wm = (WindowManager *)self->manager;
  FlintContext *ctx = FlintContext_Get();
  ctx->window.selection_complete = false;
  ctx->window.output_buffer[0] = '\0';
  data->prev_hPopupWnd = ctx->window.hPopupWnd;
  log_info("PromptPopup_OnShow title=%s x=%d y=%d popup_count=%d",
           data->spec.title ? data->spec.title : "(null)", data->x, data->y,
           wm->popup_count);

  Platform_PromptPopupShowWindow(self, data->x, data->y, data->w, data->h);

  data->state = calloc(1, sizeof(PromptState));
  data->state->editbox = calloc(1, sizeof(EditBoxInfo));
  data->state->listbox = calloc(1, sizeof(ListBoxInfo));
  if (!data->state) {
    return;
  }

  data->state->listbox->items = (char **)data->spec.items;
  data->state->listbox->item_count = 0;
  data->state->editbox->cursor = 0;
  data->state->width = data->w;
  data->state->height = data->h;
  const char *title = data->spec.title ? data->spec.title : "Flint";
  data->state->window_title = (char *)prompt_feature_name(title);
  data->state->editbox->filter[0] = '\0';
  if (data->spec.placeholder && data->spec.placeholder[0]) {
    data->state->editbox->placeholder = strdup(data->spec.placeholder);
  }
  FlintContext_Get()->window.filter[0] = '\0';
  FlintContext_Get()->window.filter_cursor = 0;
  if (data->spec.items || data->spec.refresh || data->spec.render) {
    data->state->listbox = calloc(1, sizeof(ListBoxInfo));
  } else {
    free(data->state->listbox); data->state->listbox = NULL;
  }
  ListBoxInfo_Init(data->state->listbox);
  data->state->no_bg = data->spec.no_bg;
  data->state->listbox->selected_index = -1;
  self->user_data = data->state;
  data->state->no_bg = data->spec.no_bg;

  FlintContext *ctx2 = FlintContext_Get();
  if (!wm->ctx)
    wm->ctx = ctx2;
  log_info("BEFORE SAVE: ctx=%p enter=%p ctrl_enter=%p", (void *)ctx2,
           (void *)ctx2->prompt_callbacks.enter,
           (void *)ctx2->prompt_callbacks.ctrl_enter);
  data->saved_callbacks = ctx2->prompt_callbacks;
  log_info("DEBUG_SAVE_CALLBACKS title='%s' saved_enter=%p saved_ctrl_enter=%p "
           "spec_enter=%p spec_ctrl_enter=%p",
           data->spec.title ? data->spec.title : "(null)",
           (void *)data->saved_callbacks.enter,
           (void *)data->saved_callbacks.ctrl_enter, (void *)data->spec.enter,
           (void *)data->spec.ctrl_enter);
  ctx2->prompt_callbacks.refresh =
      data->spec.refresh ? data->spec.refresh : data->saved_callbacks.refresh;
  ctx2->prompt_callbacks.enter =
      data->spec.enter ? data->spec.enter : data->saved_callbacks.enter;
  ctx2->prompt_callbacks.ctrl_enter = data->spec.ctrl_enter
                                          ? data->spec.ctrl_enter
                                          : data->saved_callbacks.ctrl_enter;
  ctx2->prompt_callbacks.shift_enter = data->spec.shift_enter
                                           ? data->spec.shift_enter
                                           : data->saved_callbacks.shift_enter;
  ctx2->prompt_callbacks.ctrl_shift_enter =
      data->spec.ctrl_shift_enter ? data->spec.ctrl_shift_enter
                                  : data->saved_callbacks.ctrl_shift_enter;
  ctx2->prompt_callbacks.ctrl_alt_enter =
      data->spec.ctrl_alt_enter ? data->spec.ctrl_alt_enter
                                : data->saved_callbacks.ctrl_alt_enter;
  ctx2->prompt_callbacks.shift_alt_enter =
      data->spec.shift_alt_enter ? data->spec.shift_alt_enter
                                 : data->saved_callbacks.shift_alt_enter;
  ctx2->prompt_callbacks.tab =
      data->spec.tab ? data->spec.tab : data->saved_callbacks.tab;
  ctx2->prompt_callbacks.render =
      data->spec.render ? data->spec.render : data->saved_callbacks.render;
  data->state->render = ctx2->prompt_callbacks.render;
  ctx2->prompt_callbacks.edit =
      data->spec.edit ? data->spec.edit : data->saved_callbacks.edit;
  ctx2->prompt_callbacks.filter =
      data->spec.filter ? data->spec.filter : data->saved_callbacks.filter;
  ctx2->prompt_callbacks.help =
      data->spec.help ? data->spec.help : data->saved_callbacks.help;
  ctx2->window.current_feature = (char *)prompt_feature_name(title);
  ctx2->window.title = title;
  ctx2->window.callbacks = ctx2->prompt_callbacks;
  History_ResetCursor(ctx2->window.current_feature);
  ctx2->window.editbox_placed = true;
  if (data->spec.refresh)
    data->spec.refresh(data->state);
}

static void PromptPopup_OnHide(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  log_info("DEBUG_ON_HIDE title='%s'",
           data->spec.title ? data->spec.title : "(null)");
  Platform_PromptPopupHideWindow(self);
  if (data->state) {
    last_index = 0;
    if (data->state->listbox)
      data->state->listbox->selected_index = 0;
    FlintContext *_c3 = FlintContext_Get();
    _c3->window.current_feature = NULL;
    _c3->prompt_callbacks = data->saved_callbacks;
    _c3->window.callbacks = data->saved_callbacks;
    _c3->window.hPopupWnd = data->prev_hPopupWnd;
  }
}

static void PromptPopup_OnRender(Popup *self) {
  PromptPopupData *data = (PromptPopupData *)self;
  if (!data->state)
    return;
  Platform_PromptPopupRender(self);
}

static bool PromptPopup_OnKey(Popup *self, const PopupKeyEvent *key) {
  return Platform_PromptPopupOnKey(self, key);
}

static void PromptPopup_Destroy(Popup *self) {

  PromptPopupData *data = (PromptPopupData *)self;
  Platform_PromptPopupDestroyWindow(self);
  if (data->state) {
    ListBoxInfo_Free(data->state->listbox);
    free(data->state->editbox->placeholder);
    free(data->state);
  }
  free(data);
}

static const PopupVTable g_prompt_popup_vtable = {
    .on_show = PromptPopup_OnShow,
    .on_hide = PromptPopup_OnHide,
    .on_render = PromptPopup_OnRender,
    .on_key = PromptPopup_OnKey,
    .destroy = PromptPopup_Destroy};

int PromptPopup_GetBounds(Popup *p, int *x, int *y, int *w, int *h) {
  if (!p || p->type != POPUP_PROMPT)
    return 0;
  PromptPopupData *d = (PromptPopupData *)p;
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

Popup *PromptPopup_Create(WindowManager *wm, const PromptSpec *spec) {
  if (!wm || !spec)
    return NULL;

  int scale = (int)Platform_GetUIScale();
  if (scale <= 0)
    scale = 1;
  int layout_w = spec->w > 0 ? spec->w : 900 * scale;
  int layout_h = spec->h > 0 ? spec->h : 640 * scale;

  {
    int mx = 0, my = 0, mw = 0, mh = 0;
    WindowManager_GetMainMonitorGeometry(&mx, &my, &mw, &mh);
    if (mw > 0) {
      if (layout_w > mw - 40)
        layout_w = mw - 40;
      if (layout_w < 100)
        layout_w = mw > 140 ? mw - 40 : mw;
    }
    if (mh > 0) {
      if (layout_h > mh - 40)
        layout_h = mh - 40;
      if (layout_h < 100)
        layout_h = mh > 140 ? mh - 40 : mh;
    }
    if (layout_w <= 0)
      layout_w = spec->w > 0 ? spec->w : 900;
    if (layout_h <= 0)
      layout_h = spec->h > 0 ? spec->h : 640;
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
    WindowManager_GetCenteredPos(layout_w, layout_h, &x, &y);
  } else {
    int wx = 0, wy = 0;
    Popup *top = WindowManager_GetTopPopup(wm);
    bool have_origin = false;
    if (top)
      have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
    if (!have_origin) {
      WindowHandle hMain = FlintContext_Get()->window.hPopupWnd;
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

  data->base.vtable = &g_prompt_popup_vtable;
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
  PromptPopupData *data = (PromptPopupData *)top;
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
