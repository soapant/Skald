#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "core/context.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "deps/log.h"
#include "core/window_manager.h"

#include "core/checkbox.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "platform/input_state.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_owner.h"
#include "core/feature_loader.h"

static WindowManager *g_window_manager = NULL;

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
  if (!Platform_WM_InitDisplay(wm)) {
    free(wm->popups);
    free(wm);
    return NULL;
  }
  Platform_ApplyUIScale();
  wm->ctx = SkaldContext_Get();
  g_window_manager = wm;
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
  if (g_window_manager == wm)
    g_window_manager = NULL;
  free(wm);
  log_info("WindowManager_Destroy completed");
}

WindowManager *WindowManager_Get(void) { return g_window_manager; }

int WindowManager_Run(WindowManager *wm) {
  if (!wm)
    return -1;
  int rc = Platform_WM_Run(wm);
  return rc;
}

static bool g_popup_push_in_progress = false;

Popup *WindowManager_PushPopup(WindowManager *wm, Popup *popup) {
  if (g_popup_push_in_progress) {
    log_info("PushPopup REENTRANT blocked");
    return NULL;
  }
  if (!wm || !popup)
    return NULL;
  Platform_InputState_InhibitMods();
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
  g_popup_push_in_progress = true;
  if (popup->vtable && popup->vtable->on_show) {
    popup->vtable->on_show(popup);
  }
  g_popup_push_in_progress = false;
  return popup;
}

void WindowManager_PopPopup(WindowManager *wm) {
  if (!wm || wm->popup_count == 0)
    return;
  Checkbox_ResetAll();
  Popup *popup = wm->popups[--wm->popup_count];
  CloseCallback on_close = PromptPopup_TakeOnClose(popup);
  if (popup->vtable && popup->vtable->on_hide) {
    popup->vtable->on_hide(popup);
  }
  popup->visible = false;
  popup->manager = NULL;
  if (popup->vtable && popup->vtable->destroy) {
    popup->vtable->destroy(popup);
  }
  if (wm->popup_count == 0) {
    Platform_WM_OnPopEmpty(wm);
  }
  if (on_close) {
    on_close();
  }
  FeatureLoader_SweepRetired();
}

Popup *WindowManager_GetTopPopup(WindowManager *wm) {
  if (!wm || wm->popup_count == 0)
    return NULL;
  return wm->popups[wm->popup_count - 1];
}

void WindowManager_GetMainMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
  Platform_WM_GetMonitorGeometry(ox, oy, ow, oh);
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

int PromptPopup_GetBounds(Popup *p, int *x, int *y, int *w, int *h);

void WindowManager_GetPromptBounds(int *out_x, int *out_y, int *out_w,
                                   int *out_h) {
  int mox = 0, moy = 0, mow = 0, moh = 0;
  WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
  Popup *top = WindowManager_GetTopPopup(NULL);
  int pw = 0, ph = 0, px = -1, py = -1;
  if (top && top->type == POPUP_PROMPT)
    PromptPopup_GetBounds(top, &px, &py, &pw, &ph);
  if (pw <= 0 || ph <= 0) {
    pw = (int)(900 * Platform_GetUIScale());
    ph = (int)(640 * Platform_GetUIScale());
    px = -1;
  }
  if (px < 0 || py < 0)
    WindowManager_GetCenteredPos(pw, ph, &px, &py);
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
  EventBus_Trigger(EVENT_PROMPT_OPEN);
  if (!wm || !spec)
    return NULL;
  if (!spec->subwindow && wm->popup_count > 0)
    return NULL;
  SkaldContext_Get()->window.editbox_placed = false;
  char *res = Platform_PromptUser(wm, spec);
  return res;
}

#define MAX_EVENT_CALLBACKS 16
struct EventSlot {
  PlEventCallback cb;
  char owner[OWNER_PATH_MAX];
};
static struct EventSlot g_event_registry[2][MAX_EVENT_CALLBACKS] = {0};

void EventBus_Register(PlEvent event, PlEventCallback cb) {
  if (event < 0 || event >= 2 || !cb)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (g_event_registry[event][i].cb == NULL) {
      g_event_registry[event][i].cb = cb;
      g_event_registry[event][i].owner[0] = '\0';
      Owner_ResolveForFunc((void *)cb, g_event_registry[event][i].owner,
                           sizeof(g_event_registry[event][i].owner));
      return;
    }
  }
}

void EventBus_Unregister(PlEvent event, PlEventCallback cb) {
  if (event < 0 || event >= 2)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (g_event_registry[event][i].cb == cb) {
      g_event_registry[event][i].cb = NULL;
      g_event_registry[event][i].owner[0] = '\0';
      return;
    }
  }
}

void EventBus_PurgeOwner(const char *path_a, const char *path_b) {
  if ((!path_a || !path_a[0]) && (!path_b || !path_b[0]))
    return;
  for (int e = 0; e < 2; ++e) {
    for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
      if (g_event_registry[e][i].cb &&
          Owner_ModuleMatches(g_event_registry[e][i].owner, path_a, path_b)) {
        g_event_registry[e][i].cb = NULL;
        g_event_registry[e][i].owner[0] = '\0';
      }
    }
  }
}

void EventBus_Trigger(PlEvent event) {
  if (event < 0 || event >= 2)
    return;
  for (int i = 0; i < MAX_EVENT_CALLBACKS; ++i) {
    if (g_event_registry[event][i].cb) {
      g_event_registry[event][i].cb();
    }
  }
}

void WindowManager_Reset(void) {
  g_popup_push_in_progress = false;
  g_window_manager = NULL;
  memset(g_event_registry, 0, sizeof(g_event_registry));
}
