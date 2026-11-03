#ifndef EDIT_STATE_H
#define EDIT_STATE_H
#include <stdbool.h>
#include "core/types.h"

typedef struct {
  bool active;
  int anchor;
  int head;
} EditSelection;

void EditState_BeginSelection(int cursor);
void EditState_UpdateSelection(int cursor);
void EditState_EndSelection(void);
bool EditState_GetSelection(int *out_start, int *out_end);

#endif
