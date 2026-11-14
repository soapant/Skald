#ifndef EDIT_STATE_H
#define EDIT_STATE_H
#include <stdbool.h>
#include "core/types.h"

typedef struct {
  bool active;
  int anchor;
  int head;
} EditSelection;

void edit_state_begin_selection(int cursor);
void edit_state_update_selection(int cursor);
void edit_state_end_selection(void);
bool edit_state_get_selection(int *out_start, int *out_end);
void edit_state_reset(void);

#endif
