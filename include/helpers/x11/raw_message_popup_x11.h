#ifndef HELPERS_X11_MESSAGE_POPUP_H
#define HELPERS_X11_MESSAGE_POPUP_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <pthread.h>
#include "core/types.h"
#include <stdlib.h>
#include <time.h>
#include <X11/X.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>
#include "helpers/x11/display_x11.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "config_values.h"
#include "core/context.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/window_manager.h"

#define MSG_MAX_ACTIVE 24
#define MSG_HOVER_DEFAULT_W 900
#define MSG_HOVER_DEFAULT_H 600
#define MSG_MAX_LINES 64
#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600
#define MSG_COLLISION_GAP 20

typedef struct {
  Window win;
  bool indefinite;
  char key[64];
  char name[64];
  int w;
  int h;
  int x;
  int y;
} MsgSlot;

typedef struct {
  const char *s;
  int n;
  int w;
} MsgLine;

extern pthread_mutex_t g_msg_lock;
extern MsgSlot g_msg_slots[MSG_MAX_ACTIVE];
extern int g_msg_slot_count;

void x11_msg_remove_slot_locked(Display *dpy, int idx);
int x11_msg_find_key_locked(const char *key);
void x11_msg_remove_indefinite_locked(Display *dpy);
int x11_msg_wrap_text(Display *dpy, XftFont *xf, const char *text, int maxW,
                      MsgLine *out, int maxLines);
int x11_msg_stack_adjust_y(int x, int y, int w, int h, int dir);
void x11_msg_present(Display *dpy, const char *message, int x, int y, int winW,
                     int winH, double timeout, ColorRef color, bool no_bg,
                     void (*render)(void), const char *name);
void x11_msg_hover_present(Display *dpy, const char *key, const char *message,
                           int centerX, int centerY, int winW, int winH,
                           ColorRef color, bool no_bg);
void x11_msg_measure(Display *dpy, const char *message, int maxWidth,
                     int maxHeight, int *outW, int *outH);

#endif
