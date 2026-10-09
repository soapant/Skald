#ifndef MODAL_H
#define MODAL_H

#include <stdbool.h>
#include "core/types.h"
#include "core/context.h"

#define g_current_mode (FlintContext_Get()->vim_mode)
#define g_vim_mode (FlintContext_Get()->vim_mode_str)
extern int last_index;
extern char* selection_feature;

void SetMode(EditorMode mode);
ColorRef Modal_CursorColor(void);
void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb);
bool IsNormalMode(void);
bool IsInsertMode(void);
bool IsVisualMode(void);
const char* Vim_ModeName(EditorMode mode);

#endif