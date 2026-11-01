#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/context.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"

typedef WMEditPopupData EditPopupDataInternal;

static void EditPopup_OnShow(Popup *self) {
  EditPopupDataInternal *data = (EditPopupDataInternal *)self;
  data->mode = MODE_INSERT;
  Platform_EditPopupShowWindow(self);
}

static void EditPopup_OnHide(Popup *self) {
  Platform_EditPopupHideWindow(self);
}

static void EditPopup_OnRender(Popup *self) { Platform_EditPopupRender(self); }

static bool EditPopup_OnKey(Popup *self, const PopupKeyEvent *key) {
    return Platform_EditPopupOnKey(self, key);
}

static void EditPopup_Destroy(Popup *self) {
  EditPopupDataInternal *data = (EditPopupDataInternal *)self;
  free(data->text);
  free(data->align);
  free(data->placeholder);
  Platform_EditPopupDestroyWindow(self);
  free(data);
}

static const PopupVTable g_edit_popup_vtable = {.on_show = EditPopup_OnShow,
                                                .on_hide = EditPopup_OnHide,
                                                .on_render = EditPopup_OnRender,
                                                .on_key = EditPopup_OnKey,
                                                .destroy = EditPopup_Destroy};

void EditPopup_SetPlaceholder(Popup *popup, const char *placeholder) {
  if (!popup || popup->type != POPUP_EDIT)
    return;
  EditPopupDataInternal *data = (EditPopupDataInternal *)popup;
  char *np = placeholder ? strdup(placeholder) : NULL;
  if (placeholder && !np)
    return;
  free(data->placeholder);
  data->placeholder = np;
  if (data->base.vtable && data->base.vtable->on_render)
    data->base.vtable->on_render(popup);
}

Popup *EditPopup_Create(WindowManager *wm, const char *initial_text, int x,
                        int y, int w, int h, const char *align,
                        EditPopupConfirmCallback on_confirm, void *user_data) {

  EditPopupDataInternal *data = calloc(1, sizeof(EditPopupDataInternal));
  if (!data)
    return NULL;
  w = (int)(w * Platform_GetUIScale());
  h = (int)(h * Platform_GetUIScale());
  WindowManager_GetCenteredPos(w, h, &x, &y);

  data->base.vtable = &g_edit_popup_vtable;
  data->base.manager = wm;
  data->base.type = POPUP_EDIT;
  data->align = align ? strdup(align) : strdup("LEFT");
  if (!data->align) {
    free(data->align);
    free(data);
    return NULL;
  }
  data->text_capacity = 1024;
  data->text = calloc(data->text_capacity, 1);
  if (!data->text) {
    free(data->align);
    free(data);
    return NULL;
  }
  if (initial_text) {
    strncpy(data->text, initial_text, data->text_capacity - 1);
    data->cursor = strlen(data->text);
  }
  data->on_confirm = on_confirm;
  data->user_data = user_data;
  data->x = x;
  data->y = y;
  data->w = w;
  data->h = h;
  data->confirmed = false;

  if (!Platform_EditPopupCreateWindow((Popup *)data, x, y, w, h)) {
    free(data->text);
    free(data->align);
    free(data);
    return NULL;
  }
  return (Popup *)data;
}
