#ifndef VIM_STATE_H
#define VIM_STATE_H

#include <stdbool.h>

#define VIM_MARK_SLOTS      27
#define VIM_JUMP_STACK      64
#define VIM_SEARCH_PATTERN  128
#define VIM_MARK_PREV       26

typedef enum {
    VIM_FIND_NONE = 0,
    VIM_FIND_F,
    VIM_FIND_T,
    VIM_FIND_BACKWARD,
} VimFindDir;

typedef struct {
    bool     valid;
    char     target;
    VimFindDir dir;
} VimFindState;

typedef struct {
    bool valid;
    int  cursor;
    int  selection;
} VimMark;

typedef struct {
    int cursor;
    int selection;
} VimJumpEntry;

typedef struct {
    bool active;
    int  anchor;
    int  head;
} VimSelection;

void VimState_Reset(void);

void VimState_SetFind(char target, VimFindDir dir);
bool VimState_GetFind(VimFindState* out);
VimFindDir VimState_InvertDir(VimFindDir dir);

void VimState_BeginSelection(int cursor);
void VimState_UpdateSelection(int cursor);
void VimState_EndSelection(void);
bool VimState_GetSelection(int* out_start, int* out_end);
bool VimState_HasSelection(void);

void VimState_SetLastPattern(const char* pattern);
bool VimState_GetLastPattern(char* out, int cap);

void VimState_SetMark(char name, int cursor, int selection);
bool VimState_GetMark(char name, int* out_cursor, int* out_selection);
bool VimState_MarkIsSet(char name);

void VimState_PushJump(int cursor, int selection);
bool VimState_PopJump(int* out_cursor, int* out_selection);
bool VimState_PeekJump(int* out_cursor, int* out_selection);

void VimState_SetLastInsert(int cursor);
int  VimState_GetLastInsert(void);

void VimState_BeginSearchPattern(void);
void VimState_AppendSearchPattern(char c);
void VimState_EndSearchPattern(const char** out_pattern);
bool VimState_SearchPending(void);

#endif
