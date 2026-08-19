#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <stdbool.h>
#include "core/types.h"

#define FILE_NVIM_ZOXIDE 2
#define NvimZoxideFile G_FILE_ENTRIES[FILE_NVIM_ZOXIDE].mf

extern char* TextEditorName;
extern bool Initialized;

void TriggerNvimZoxide(void);

#endif
