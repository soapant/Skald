#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "core/types.h"
#include "core/context.h"

typedef struct Popup Popup;
typedef struct WindowManager WindowManager;

typedef enum {
  EVENT_PROMPT_OPEN,
  EVENT_PROMPT_CLOSE,
} PlEvent;

typedef void (*PlEventCallback)(void);

typedef struct {
  void (*on_show)(Popup *self);
  void (*on_hide)(Popup *self);
  void (*on_render)(Popup *self);
  bool (*on_key)(Popup *self, const PopupKeyEvent *key);
  void (*on_focus_lost)(Popup *self);
  void (*destroy)(Popup *self);
} PopupVTable;
typedef enum { POPUP_PROMPT, POPUP_MESSAGE } PopupType;

struct Popup {
  const PopupVTable *vtable;
  WindowHandle handle;
  WindowManager *manager;
  bool visible;
  void *user_data;
  PopupType type;
};

struct WindowManager {
  Popup **popups;
  int popup_count;
  int popup_capacity;
  bool running;
  SkaldContext *ctx;

#if defined(__linux__)
  Display *display;
#elif defined(_WIN32)
  void *display;
#endif
};

WindowManager *WindowManager_Create(void);
void WindowManager_Destroy(WindowManager *wm);
int WindowManager_Run(WindowManager *wm);
WindowManager *WindowManager_Get(void);
void WindowManager_Reset(void);

Popup *WindowManager_PushPopup(WindowManager *wm, Popup *popup);
void WindowManager_PopPopup(WindowManager *wm);
void WindowManager_DrainSelectionComplete(WindowManager *wm);
Popup *WindowManager_GetTopPopup(WindowManager *wm);
bool WindowManager_TopPopupWantsTick(WindowManager *wm);

char *WindowManager_ShowPrompt(WindowManager *wm, const PromptSpec *spec);

Popup *PromptPopup_Create(WindowManager *wm, const PromptSpec *spec);
int PromptPopup_GetBounds(Popup *p, int *x, int *y, int *w, int *h);
CloseCallback PromptPopup_TakeOnClose(Popup *p);
PromptState *PromptPopup_FindState(const ListBoxInfo *lb);
Popup *PromptPopup_FindPopupByHandle(WindowHandle handle);
PromptState *PromptPopup_GetTopPromptState(void);

void WindowManager_GetMainMonitorGeometry(int *x, int *y, int *w, int *h);
void WindowManager_GetCenteredPos(int win_w, int win_h, int *out_x, int *out_y);
void WindowManager_GetPromptBounds(int *out_x, int *out_y, int *out_w,
                                   int *out_h);

void WindowManager_Quit(WindowManager *wm);
void WindowManager_ProcessEvents(WindowManager *wm);
bool WindowManager_IsPointInTopPopup(WindowManager *wm, int root_x, int root_y);

void EventBus_Register(PlEvent event, PlEventCallback cb);
void EventBus_Unregister(PlEvent event, PlEventCallback cb);
void EventBus_PurgeOwner(const char *path_a, const char *path_b);
void EventBus_Trigger(PlEvent event);

#endif
