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
void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb);
EditorMode GetMode(void);
bool IsNormalMode(void);
bool IsInsertMode(void);
bool IsVisualMode(void);

#endif
