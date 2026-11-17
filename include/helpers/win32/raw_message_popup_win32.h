#ifndef RAW_MESSAGE_POPUP_WIN32_H
#define RAW_MESSAGE_POPUP_WIN32_H

#include <windows.h>
#include <stdbool.h>
#include "platform/platform.h"
#include "helpers/win32/subclasses_win32.h"
#include <stdlib.h>
#include <string.h>
#include "config_values.h"
#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/bg_image_win32.h"
#include "wingdi.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_ui.h"

#define MSG_SLOT_MAX 24
#define MSG_COLLISION_GAP 20
#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600
#define MSG_TEXT_INSET_X 15
#define MSG_TEXT_INSET_Y 15
#define MSG_BOTTOM_CLIP 5

extern HWND g_msg_slots[MSG_SLOT_MAX];
extern int g_msg_slot_count;

void win32_msg_created_clear(void);
void win32_msg_hover_release(const char *key);
void win32_msg_hover_present(PlHoverMessageSpec spec);

enum { MSG_DESTROY_ALL, MSG_DESTROY_INDEFINITE, MSG_DESTROY_NAMED };

int win32_msg_slot_find(HWND hwnd);
void win32_msg_slot_unregister(HWND hwnd);
void win32_msg_slot_register(HWND hwnd);
bool win32_message_popup_any_open(void);
int win32_msg_slot_destroy_where(int mode, const char *name);
bool win32_message_popup_create(int x, int y, int w, int h);
extern bool s_msg_rounded;
extern int s_msg_radius;

extern FadeWindowData *s_msg_show_data;
extern bool s_msg_show_no_bg;

#endif
