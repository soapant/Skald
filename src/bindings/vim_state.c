#include "bindings/vim_state.h"
#include <string.h>

static VimFindState    g_find;
static VimMark         g_marks[VIM_MARK_SLOTS];
static VimJumpEntry    g_jumps[VIM_JUMP_STACK];
static int             g_jump_count;
static int             g_jump_pos;
static int             g_last_insert = -1;
static char            g_search_pattern[VIM_SEARCH_PATTERN];
static int             g_search_len;
static bool            g_search_pending;
static char            g_last_pattern[VIM_SEARCH_PATTERN];

static int MarkIndex(char name) {
    if (name >= 'a' && name <= 'z') return name - 'a';
    if (name >= 'A' && name <= 'Z') return name - 'A';
    if (name == VIM_MARK_PREV) return VIM_MARK_PREV;
    return -1;
}

void VimState_Reset(void) {
    memset(&g_find, 0, sizeof(g_find));
    memset(g_marks, 0, sizeof(g_marks));
    g_jump_count = 0;
    g_jump_pos = 0;
    g_last_insert = -1;
    g_search_len = 0;
    g_search_pending = false;
    g_search_pattern[0] = '\0';
    g_last_pattern[0] = '\0';
}

void VimState_SetFind(char target, VimFindDir dir) {
    g_find.valid = true;
    g_find.target = target;
    g_find.dir = dir;
}

bool VimState_GetFind(VimFindState* out) {
    if (!g_find.valid || !out) return false;
    *out = g_find;
    return true;
}

void VimState_SetLastPattern(const char* pattern) {
    if (!pattern) { g_last_pattern[0] = '\0'; return; }
    strncpy(g_last_pattern, pattern, sizeof(g_last_pattern) - 1);
    g_last_pattern[sizeof(g_last_pattern) - 1] = '\0';
}

bool VimState_GetLastPattern(char* out, int cap) {
    if (!out || cap <= 0) return false;
    if (g_last_pattern[0] == '\0') return false;
    strncpy(out, g_last_pattern, cap - 1);
    out[cap - 1] = '\0';
    return true;
}

VimFindDir VimState_InvertDir(VimFindDir dir) {
    if (dir == VIM_FIND_BACKWARD) return VIM_FIND_T;
    if (dir == VIM_FIND_T) return VIM_FIND_BACKWARD;
    return VIM_FIND_T;
}

void VimState_SetMark(char name, int cursor, int selection) {
    int idx = MarkIndex(name);
    if (idx < 0) return;
    g_marks[idx].valid = true;
    g_marks[idx].cursor = cursor;
    g_marks[idx].selection = selection;
}

bool VimState_MarkIsSet(char name) {
    int idx = MarkIndex(name);
    if (idx < 0) return false;
    return g_marks[idx].valid;
}

bool VimState_GetMark(char name, int* out_cursor, int* out_selection) {
    int idx = MarkIndex(name);
    if (idx < 0 || !g_marks[idx].valid) return false;
    if (out_cursor) *out_cursor = g_marks[idx].cursor;
    if (out_selection) *out_selection = g_marks[idx].selection;
    return true;
}

void VimState_PushJump(int cursor, int selection) {
    if (g_jump_pos < g_jump_count) {
        g_jump_count = g_jump_pos + 1;
    }
    if (g_jump_count >= VIM_JUMP_STACK) {
        memmove(&g_jumps[0], &g_jumps[1], sizeof(VimJumpEntry) * (VIM_JUMP_STACK - 1));
        g_jump_count = VIM_JUMP_STACK - 1;
    }
    g_jumps[g_jump_count].cursor = cursor;
    g_jumps[g_jump_count].selection = selection;
    g_jump_count++;
    g_jump_pos = g_jump_count - 1;
}

bool VimState_PopJump(int* out_cursor, int* out_selection) {
    if (g_jump_pos <= 0) return false;
    g_jump_pos--;
    if (out_cursor) *out_cursor = g_jumps[g_jump_pos].cursor;
    if (out_selection) *out_selection = g_jumps[g_jump_pos].selection;
    return true;
}

bool VimState_PeekJump(int* out_cursor, int* out_selection) {
    if (g_jump_pos + 1 >= g_jump_count) return false;
    g_jump_pos++;
    if (out_cursor) *out_cursor = g_jumps[g_jump_pos].cursor;
    if (out_selection) *out_selection = g_jumps[g_jump_pos].selection;
    return true;
}

void VimState_SetLastInsert(int cursor) {
    g_last_insert = cursor;
}

int VimState_GetLastInsert(void) {
    return g_last_insert;
}

void VimState_BeginSearchPattern(void) {
    g_search_len = 0;
    g_search_pending = true;
    g_search_pattern[0] = '\0';
}

void VimState_AppendSearchPattern(char c) {
    if (!g_search_pending) return;
    if (g_search_len >= VIM_SEARCH_PATTERN - 1) return;
    g_search_pattern[g_search_len++] = c;
    g_search_pattern[g_search_len] = '\0';
}

void VimState_EndSearchPattern(const char** out_pattern) {
    if (!g_search_pending) {
        if (out_pattern) *out_pattern = NULL;
        return;
    }
    g_search_pending = false;
    if (out_pattern) *out_pattern = g_search_pattern;
}

bool VimState_SearchPending(void) {
    return g_search_pending;
}
