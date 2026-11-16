#ifndef SUBCLASSES_WIN32_H
#define SUBCLASSES_WIN32_H

#include "core/types.h"
#include "helpers/helpers_owner.h"

#define WM_DISPLAY_MESSAGE (WM_APP + 2)
#define WM_DISMISS_MESSAGE (WM_APP + 3)
#define WM_PLAYER_CMD (WM_USER + 1)

typedef struct {
  const char *message;
  FontHandle hFont;
  int alpha;
  ColorRef color;
  bool no_bg;
  void (*render)(void);
  char render_owner[OWNER_PATH_MAX];
  double timeout;
  ULONGLONG deadline_ms;
  char name[64];
} FadeWindowData;

typedef struct {
  const char *name;
} DismissMessageParams;

#include "platform/platform.h"
typedef PlCreateDisplayMessageWindowSpec DisplayMessageParams;

LRESULT CALLBACK win32_popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam);
LRESULT CALLBACK win32_display_message_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                            LPARAM lParam);
LRESULT CALLBACK win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam);

#endif
