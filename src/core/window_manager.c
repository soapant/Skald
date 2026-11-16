#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "core/context.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include "config_values.h"

#include "core/checkbox.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_input_state.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include "deps/stb_ds.h"
#pragma GCC diagnostic pop
#include "helpers/helpers_owner.h"
#include "helpers/helpers_ui.h"
#include "core/feature_loader.h"

static WindowManager *window_manager = NULL;

WindowManager *WindowManager_Create(void) {
  WindowManager *wm = calloc(1, sizeof(WindowManager));
  if (!wm)
    return NULL;
  wm->popup_capacity = 8;
  wm->popups = calloc(wm->popup_capacity, sizeof(Popup *));
  if (!wm->popups) {
    free(wm);
    return NULL;
  }
  if (!Platform_InitDisplay(wm)) {
    free(wm->popups);
    free(wm);
    return NULL;
  }
  Platform_ApplyUIScale();
  wm->ctx = SkaldContext_Get();
  window_manager = wm;
  return wm;
}

void WindowManager_Destroy(WindowManager *wm) {
  if (!wm) {
    return;
  }
  for (int i = 0; i < wm->popup_count; i++) {
    if (wm->popups[i] && wm->popups[i]->vtable &&
        wm->popups[i]->vtable->destroy) {
      wm->popups[i]->vtable->destroy(wm->popups[i]);
    }
  }
  free(wm->popups);
  if (window_manager == wm)
    window_manager = NULL;
  free(wm);
  log_info("WindowManager_Destroy completed");
}

WindowManager *WindowManager_Get(void) { return window_manager; }

static bool popup_push_in_progress = false;

Popup *WindowManager_PushPopup(WindowManager *wm, Popup *popup) {
  if (popup_push_in_progress) {
    log_info("PushPopup REENTRANT blocked");
    return NULL;
  }
  if (!wm || !popup)
    return NULL;
  input_state_inhibit_mods();
  log_info("PushPopup wm=%p popup=%p count_before=%d", (void *)wm,
           (void *)popup, wm->popup_count);
  if (wm->popup_count >= wm->popup_capacity) {
    int new_cap = wm->popup_capacity * 2;
    Popup **new_popups = realloc(wm->popups, new_cap * sizeof(Popup *));
    if (!new_popups)
      return NULL;
    wm->popups = new_popups;
    wm->popup_capacity = new_cap;
  }
  popup->manager = wm;
  popup->visible = true;
  wm->popups[wm->popup_count++] = popup;
  popup_push_in_progress = true;
  if (popup->vtable && popup->vtable->on_show) {
    popup->vtable->on_show(popup);
  }
  popup_push_in_progress = false;
  Platform_InvalidatePrompt();
  return popup;
}

void WindowManager_PopPopup(WindowManager *wm) {
  if (!wm || wm->popup_count == 0)
    return;
  checkbox_reset_all();
  Popup *popup = wm->popups[--wm->popup_count];
  SkaldContext *ctx = SkaldContext_Get();
  if (ctx)
    ctx->window.last_popup_pop_ms = Platform_GetWallClockMs();
  log_info("PopPopup wm=%p popup=%p remaining=%d title='%s'", (void *)wm,
           (void *)popup, wm->popup_count,
           ctx && ctx->window.title ? ctx->window.title : "(null)");
  CloseCallback on_close = prompt_popup_take_on_close(popup);
  if (popup->vtable && popup->vtable->on_hide) {
    popup->vtable->on_hide(popup);
  }
  popup->visible = false;
  popup->manager = NULL;
  if (popup->vtable && popup->vtable->destroy) {
    popup->vtable->destroy(popup);
  }
  if (wm->popup_count == 0) {
    Platform_OnPopEmpty();
  }
  if (on_close) {
    on_close();
  }
  feature_loader_sweep_retired();
}

Popup *WindowManager_GetTopPopup(const WindowManager *wm) {
  if (!wm || wm->popup_count == 0)
    return NULL;
  return wm->popups[wm->popup_count - 1];
}

void WindowManager_DrainSelectionComplete(WindowManager *wm) {
  if (!wm || wm->popup_count == 0)
    return;
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx || !ctx->window.selection_complete)
    return;
  ctx->window.selection_complete = false;
  WindowManager_PopPopup(wm);
  Popup *top_after = WindowManager_GetTopPopup(wm);
  if (top_after && top_after->vtable && top_after->vtable->on_render)
    top_after->vtable->on_render(top_after);
}

void WindowManager_GetDefaultPromptPosition(int win_w, int win_h, int *out_x,
                                            int *out_y) {
  int cx = 0, cy = 0;
  if (PromptX >= 0 && PromptY >= 0) {
    if (out_x)
      *out_x = PromptX;
    if (out_y)
      *out_y = PromptY;
    return;
  }
  WindowManager_GetCenteredPos(win_w, win_h, &cx, &cy);
  if (out_x)
    *out_x = PromptX >= 0 ? PromptX : cx;
  if (out_y)
    *out_y = PromptY >= 0 ? PromptY : cy;
}

void WindowManager_GetMainMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
  Platform_GetMonitorGeometry(ox, oy, ow, oh);
}

void WindowManager_GetCenteredPos(int win_w, int win_h, int *out_x,
                                  int *out_y) {
  int mox, moy, mow, moh;
  WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
  if (out_x)
    *out_x = mox + (mow - win_w) / 2;
  if (out_y)
    *out_y = moy + (moh - win_h) / 2;
}

void WindowManager_GetPromptBounds(int *out_x, int *out_y, int *out_w,
                                   int *out_h) {
  int mox = 0, moy = 0, mow = 0, moh = 0;
  WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
  Popup *top = WindowManager_GetTopPopup(WindowManager_Get());
  int pw = 0, ph = 0, px = -1, py = -1;
  if (top && top->type == POPUP_PROMPT)
    prompt_popup_get_bounds(top, &px, &py, &pw, &ph);
  if (pw <= 0 || ph <= 0) {
    pw = (int)(PromptWidth * Platform_GetUIScale());
    ph = (int)(PromptHeight * Platform_GetUIScale());
    px = -1;
  }
  if (px < 0 || py < 0)
    WindowManager_GetDefaultPromptPosition(pw, ph, &px, &py);
  if (out_x)
    *out_x = px;
  if (out_y)
    *out_y = py;
  if (out_w)
    *out_w = pw;
  if (out_h)
    *out_h = ph;
}

char *WindowManager_ShowPrompt(WindowManager *wm, const PromptSpec *spec) {
  if (!wm || !spec)
    return NULL;
  if (!spec->subwindow && wm->popup_count > 0)
    return NULL;
  SkaldContext *ctx = SkaldContext_Get();
  if (ctx)
    ctx->window.editbox_placed = false;
  event_bus_trigger(EVENT_PROMPT_OPEN);
  return prompt_user_run(wm, spec);
}

#define MAX_EVENT_CALLBACKS 16
struct EventSlot {
  EventCallback cb;
  char owner[OWNER_PATH_MAX];
};
static struct EventSlot event_registry[2][MAX_EVENT_CALLBACKS] = {0};

void event_bus_register(Event event, EventCallback cb) {
  if (event < 0 || event >= 2 || !cb)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (event_registry[event][i].cb == NULL) {
      event_registry[event][i].owner[0] = '\0';
      owner_resolve_for_func((void *)cb, event_registry[event][i].owner,
                             sizeof(event_registry[event][i].owner));
      if (event_registry[event][i].owner[0] == '\0') {
        log_warn("event_bus_register: cannot resolve owner, dropping callback");
        return;
      }
      event_registry[event][i].cb = cb;
      return;
    }
  }
  log_error(
      "event_bus_register: event %d table is full (%d slots), callback dropped",
      event, MAX_EVENT_CALLBACKS);
}

void event_bus_unregister(Event event, EventCallback cb) {
  if (event < 0 || event >= 2)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (event_registry[event][i].cb == cb) {
      event_registry[event][i].cb = NULL;
      event_registry[event][i].owner[0] = '\0';
      return;
    }
  }
}

void event_bus_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  for (int e = 0; e < 2; ++e) {
    for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
      if (event_registry[e][i].cb &&
          owner_module_matches(event_registry[e][i].owner, path_a, path_b)) {
        event_registry[e][i].cb = NULL;
        event_registry[e][i].owner[0] = '\0';
      }
    }
  }
}

void event_bus_trigger(Event event) {
  if (event < 0 || event >= 2)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (event_registry[event][i].cb) {
      event_registry[event][i].cb();
    }
  }
}

void WindowManager_Reset(void) {
  popup_push_in_progress = false;
  window_manager = NULL;
  memset(event_registry, 0, sizeof(event_registry));
}
