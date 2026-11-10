#ifndef HELPERS_WIN32_MESSAGE_POPUP_H
#define HELPERS_WIN32_MESSAGE_POPUP_H

#include <windows.h>
#include <stdbool.h>
#include "helpers/win32/subclasses_win32.h"

#define MSG_SLOT_MAX 24
#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600
#define MSG_TEXT_INSET_X 15
#define MSG_TEXT_INSET_Y 15
#define MSG_BOTTOM_CLIP 5

typedef struct FadeWindowData FadeWindowDataFwd;

extern HWND g_msg_slots[MSG_SLOT_MAX];
extern int g_msg_slot_count;
extern HWND g_hover_msg_hwnd;
extern HWND g_last_created_msg_hwnd;
extern int g_win32_msg_w;
extern int g_win32_msg_h;
extern char g_hover_key[64];

enum { MSG_DESTROY_ALL, MSG_DESTROY_INDEFINITE, MSG_DESTROY_NAMED };

int win32_msg_slot_find(HWND hwnd);
void win32_msg_slot_unregister(HWND hwnd);
void win32_msg_slot_register(HWND hwnd);
bool win32_message_popup_any_open(void);
int win32_msg_slot_destroy_where(int mode, const char *name);
bool message_popup_create(int x, int y, int w, int h);

extern FadeWindowData *s_msg_show_data;
extern bool s_msg_show_no_bg;

#endif
