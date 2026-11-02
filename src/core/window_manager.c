#include <stdlib.h>
#include <stdbool.h>

#include "core/context.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include "core/flapi.h"

#include "core/checkbox.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "core/types.h"
#include "deps/stb_ds.h"

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
  wm->ctx = FlintContext_Get();
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
  if (popup->vtable && popup->vtable->destroy) {
    popup->vtable->destroy(popup);
  } else {
    popup->visible = false;
    popup->manager = NULL;
  }
  if (wm->popup_count == 0) {
    Platform_WM_OnPopEmpty(wm);
  }
  if (on_close) {
    on_close();
  }
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
  log_info("WindowManager_ShowPrompt called wm=%p spec=%p title='%s'",
           (void *)wm, (void *)spec,
           spec && spec->title ? spec->title : "(null)");
  if (!wm || !spec)
    return NULL;
  FlintContext_Get()->window.editbox_placed = false;
  CloseCallback on_close = spec->on_close;
  if (FlintContext_Get()->item_index_map) {
    shfree(FlintContext_Get()->item_index_map);
    FlintContext_Get()->item_index_map = NULL;
  }
  if (spec->items) {
    int count = 0;
    for (count = 0; count < MAX_VISIBLE_ITEMS && spec->items[count]; count++) {
    }
    if (count > 0) {
      FlintContext_Get()->item_index_map = NULL;
      sh_new_strdup(FlintContext_Get()->item_index_map);
      for (int i = 0; i < count; i++) {
        if (spec->items[i])
          shput(FlintContext_Get()->item_index_map, (char *)spec->items[i],
                (void *)(size_t)(i + 1));
      }
    }
  }
  char *res = Platform_PromptUser(wm, spec);
  return res;
}
