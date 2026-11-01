#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"

typedef struct {
  char *key;
  void *value;
} HashMap;

typedef enum {
  MODE_NORMAL,
  MODE_INSERT,
  MODE_VISUAL,
} EditorMode;

typedef struct {
  RefreshCallback refresh;
  EnterCallback enter;
  CtrlEnterCallback ctrl_enter;
  ShiftEnterCallback shift_enter;
  CtrlShiftEnterCallback ctrl_shift_enter;
  CtrlAltEnterCallback ctrl_alt_enter;
  ShiftAltEnterCallback shift_alt_enter;
  TabCallback tab;
  HelpCallback help;
  RenderCallback render;
  EditCallback edit;
  FilterCallback filter;
} PromptCallbacks;

typedef struct {
  WindowHandle hGlobalTrayWnd;
  WindowHandle hPopupWnd;
  WindowHandle hDisplayMessage;
  bool selection_complete;
  bool editbox_placed;
  char output_buffer[256];
  char filter[1024];
  int filter_cursor;
  char *current_feature;
  const char *title;
  int last_index;
  PromptCallbacks callbacks;
} WindowContext;

typedef struct {
  EditorMode vim_mode;
  const char *vim_mode_str;
  uint32_t main_thread_id;
  WindowContext window;
  PromptCallbacks prompt_callbacks;
  void *win32_keyboard_hook;
  void *win32_mouse_hook;
  HashMap *feature_history;
  HashMap *item_index_map;
  HashMap *history_indices;
} FlintContext;

FlintContext *FlintContext_Get(void);

#endif
