#ifndef EDIT_STATE_H
#define EDIT_STATE_H

#include <stdbool.h>

typedef enum {
  EDIT_FIND_NONE = 0,
  EDIT_FIND_F,
  EDIT_FIND_T,
  EDIT_FIND_BACKWARD,
} EditFindDir;

typedef struct {
  bool valid;
  char target;
  EditFindDir dir;
} EditFindState;

typedef struct {
  bool valid;
  int cursor;
  int selection;
} EditMark;

typedef struct {
  int cursor;
  int selection;
} EditJumpEntry;

typedef struct {
  bool active;
  int anchor;
  int head;
} EditSelection;

void EditState_Reset(void);


void EditState_BeginSelection(int cursor);
void EditState_UpdateSelection(int cursor);
void EditState_EndSelection(void);
bool EditState_GetSelection(int *out_start, int *out_end);
bool EditState_HasSelection(void);

#endif
