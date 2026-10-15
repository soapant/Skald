#include "core/window_manager.h"
#include "core/timer.h"

Popup* SearchPopup_Create(WindowManager* wm, const char* title, int x, int y, int w, int h,
                          EditPopupConfirmCallback on_confirm, void* user_data) {
    PROFILE_START(timer);
    (void)wm; (void)title; (void)x; (void)y; (void)w; (void)h;
    (void)on_confirm; (void)user_data;
    PROFILE_END_VAR(timer, "window_manager:SearchPopup_Create");
    return NULL;
}
