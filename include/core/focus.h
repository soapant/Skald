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

void focus_frame_begin(bool include_listbox);
void focus_frame_end(void);
void focus_register(const char *name, FocusKind kind, int x, int y, int w,
                    int h, void *owner, FocusActivateFn activate,
                    FocusAdjustFn adjust);
bool focus_is(const char *name);
bool focus_has_entries(void);
FocusKind focus_kind(void);
const char *focus_name(void);
bool focus_select(const char *name);
bool focus_move(int dir);
bool focus_activate(void);
bool focus_adjust(int dir);
void focus_set_pseudo(FocusKind kind);
void focus_set_pseudo_passive(FocusKind kind);
void focus_clear(void);

#endif
