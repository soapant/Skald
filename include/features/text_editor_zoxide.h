#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <stdbool.h>
#include "core/types.h"

#define FILE_TEXT_EDITOR_ZOXIDE 2
#define TextEditorFile G_FILE_ENTRIES[FILE_TEXT_EDITOR_ZOXIDE].mf

extern char* TextEditorName;
extern bool Initialized;

void TriggerTextEditorZoxide(void);

#endif
