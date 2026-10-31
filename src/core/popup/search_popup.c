#include <stddef.h>

#include "core/window_manager.h"

Popup *SearchPopup_Create(WindowManager *wm, const char *title, int x, int y,
                          int w, int h, EditPopupConfirmCallback on_confirm,
                          void *user_data) {

  (void)wm;
  (void)title;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)on_confirm;
  (void)user_data;

  return NULL;
}
