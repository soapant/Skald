#ifndef CORE_FOCUS_H
#define CORE_FOCUS_H

#include <stdbool.h>

typedef enum {
  FOCUS_KIND_NONE = 0,
  FOCUS_KIND_BUTTON,
  FOCUS_KIND_CHECKBOX,
  FOCUS_KIND_SLIDER,
  FOCUS_KIND_LISTBOX,
  FOCUS_KIND_EDITBOX,
  FOCUS_KIND_CUSTOM
} FocusKind;

typedef void (*FocusActivateFn)(void *owner);
typedef void (*FocusAdjustFn)(void *owner, int dir);

typedef enum {
  FOCUS_LEFT = 0,
  FOCUS_RIGHT,
  FOCUS_UP,
  FOCUS_DOWN
} FocusDirection;

typedef struct {
  bool f_include_listbox;
  int f_lb_x;
  int f_lb_y;
  int f_lb_w;
  int f_lb_h;
} FocusFrameBegin;

void focus_frame_begin(FocusFrameBegin *argspec);
void focus_frame_end(void);
typedef struct {
  const char *f_name;
  FocusKind f_kind;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  void *f_owner;
  FocusActivateFn f_activate;
  FocusAdjustFn f_adjust;
} FocusRegister;

void focus_register(FocusRegister *argspec);
bool focus_is(const char *name);
bool focus_has_entries(void);
FocusKind focus_kind(void);
const char *focus_name(void);
bool focus_select(const char *name);
bool focus_set_kind(FocusKind kind);
bool focus_move(int dir);
bool focus_move_direction(FocusDirection dir);
bool focus_activate(void);
bool focus_adjust(int dir);
void focus_set_pseudo(FocusKind kind);
void focus_set_pseudo_named(FocusKind kind, const char *name);
void focus_set_pseudo_passive(FocusKind kind);
void focus_clear(void);

#endif

