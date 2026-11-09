#include "core/edit_state.h"
#include <stdbool.h>
#include <string.h>

#include "core/context.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

static EditSelection g_sel;

void EditState_BeginSelection(int cursor) {
  g_sel.active = true;
  g_sel.anchor = cursor;
  g_sel.head = cursor;
}

void EditState_UpdateSelection(int cursor) {
  if (!g_sel.active)
    return;
  g_sel.head = cursor;
}

void EditState_EndSelection(void) {
  g_sel.active = false;
  g_sel.anchor = 0;
  g_sel.head = 0;
}

void EditState_Reset(void) {
  g_sel.active = false;
  g_sel.anchor = 0;
  g_sel.head = 0;
}

bool EditState_GetSelection(int *out_start, int *out_end) {
  if (!g_sel.active)
    return false;
  if (g_sel.anchor == g_sel.head)
    return false;
  int a = g_sel.anchor < g_sel.head ? g_sel.anchor : g_sel.head;
  int b = g_sel.anchor < g_sel.head ? g_sel.head : g_sel.anchor;
  if (out_start)
    *out_start = a;
  if (out_end)
    *out_end = b;
  return true;
}
