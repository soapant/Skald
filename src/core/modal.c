#include <stdbool.h>
#include <string.h>

#include "core/context.h"
#include "core/modal.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/vim.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

int last_index = 0;


static void clamp_cursor_for_mode(EditorMode mode) {
  if (mode != MODE_NORMAL)
    return;
  FlintContext *ctx = FlintContext_Get();
  if (!ctx)
    return;
  char text[1024];
  Edit_GetText(ctx, text, sizeof(text));
  int len = (int)strlen(text);
  if (len <= 0)
    return;
  if (Edit_GetCursor(ctx) > len - 1)
    Edit_SetCursor(ctx, len - 1);
}

#include "bindings/vim_state.h"
#include <string.h>

static EditFindState g_find;
static EditMark g_marks[EDIT_MARK_SLOTS];
static EditJumpEntry g_jumps[EDIT_JUMP_STACK];
static EditSelection g_sel;

static int MarkIndex(char name) {
  if (name >= 'a' && name <= 'z')
    return name - 'a';
  if (name >= 'A' && name <= 'Z')
    return name - 'A';
  if (name == EDIT_MARK_PREV)
    return EDIT_MARK_PREV;
  return -1;
}

void EditState_Reset(void) {
  memset(&g_find, 0, sizeof(g_find));
  memset(g_marks, 0, sizeof(g_marks));
  g_sel.active = false;
  g_sel.anchor = 0;
  g_sel.head = 0;
}

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

bool EditState_HasSelection(void) { return g_sel.active; }

void EditState_SetFind(char target, EditFindDir dir) {
  g_find.valid = true;
  g_find.target = target;
  g_find.dir = dir;
}

bool EditState_GetFind(EditFindState *out) {
  if (!g_find.valid || !out)
    return false;
  *out = g_find;
  return true;
}



void EditState_SetMark(char name, int cursor, int selection) {
  int idx = MarkIndex(name);
  if (idx < 0)
    return;
  g_marks[idx].valid = true;
  g_marks[idx].cursor = cursor;
  g_marks[idx].selection = selection;
}

bool EditState_MarkIsSet(char name) {
  int idx = MarkIndex(name);
  if (idx < 0)
    return false;
  return g_marks[idx].valid;
}

bool EditState_GetMark(char name, int *out_cursor, int *out_selection) {
  int idx = MarkIndex(name);
  if (idx < 0 || !g_marks[idx].valid)
    return false;
  if (out_cursor)
    *out_cursor = g_marks[idx].cursor;
  if (out_selection)
    *out_selection = g_marks[idx].selection;
  return true;
}







