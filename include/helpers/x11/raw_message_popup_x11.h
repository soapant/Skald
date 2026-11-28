#ifndef RAW_MESSAGE_POPUP_X11_H
#define RAW_MESSAGE_POPUP_X11_H

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
  int fps;
  int last_frame_ms;
  unsigned long color;
  bool no_bg;
  char *message;
  void (*render)(void *ud);
  void *render_data;
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
typedef struct {
  Display *f_dpy;
  XftFont *f_xf;
  const char *f_text;
  int f_max_w;
  MsgLine *f_out;
  int f_maxLines;
} FnX11MsgWrapTextSpec;

int x11_msg_wrap_text(FnX11MsgWrapTextSpec *argspec);
typedef struct {
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  int f_dir;
} FnX11MsgStackAdjustYSpec;

int x11_msg_stack_adjust_y(FnX11MsgStackAdjustYSpec *argspec);
typedef struct {
  Display *f_dpy;
  const char *f_message;
  int f_x;
  int f_y;
  int f_win_w;
  int f_win_h;
  double f_timeout;
  ColorRef f_color;
  bool f_no_bg;
  void (*f_render)(void *ud);
  void *f_render_data;
  const char *f_name;
  bool f_rounded;
  int f_radius;
  int f_fps;
} FnX11MsgPresentSpec;

void x11_msg_present(FnX11MsgPresentSpec *argspec);
typedef struct {
  Display *f_dpy;
  const char *f_key;
  const char *f_message;
  int f_center_x;
  int f_center_y;
  int f_win_w;
  int f_win_h;
  ColorRef f_color;
  bool f_no_bg;
} FnX11MsgHoverPresentSpec;

void x11_msg_hover_present(FnX11MsgHoverPresentSpec *argspec);
typedef struct {
  Display *f_dpy;
  const char *f_message;
  int f_max_width;
  int f_max_height;
  int *f_out_w;
  int *f_out_h;
} FnX11MsgMeasureSpec;

void x11_msg_measure(FnX11MsgMeasureSpec *argspec);

#endif
