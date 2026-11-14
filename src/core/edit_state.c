#include "core/edit_state.h"
#include <stdbool.h>

static EditSelection sel;

void edit_state_begin_selection(int cursor) {
  sel.active = true;
  sel.anchor = cursor;
  sel.head = cursor;
}

void edit_state_update_selection(int cursor) {
  if (!sel.active)
    return;
  sel.head = cursor;
}

void edit_state_end_selection(void) {
  sel.active = false;
  sel.anchor = 0;
  sel.head = 0;
}

void edit_state_reset(void) {
  sel.active = false;
  sel.anchor = 0;
  sel.head = 0;
}

bool edit_state_get_selection(int *out_start, int *out_end) {
  if (!sel.active)
    return false;
  if (sel.anchor == sel.head)
    return false;
  int a = sel.anchor < sel.head ? sel.anchor : sel.head;
  int b = sel.anchor < sel.head ? sel.head : sel.anchor;
  if (out_start)
    *out_start = a;
  if (out_end)
    *out_end = b;
  return true;
}
