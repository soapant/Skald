#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdbool.h>
#include "core/types.h"

#define FILE_EXPLORER_ZOXIDE 4
#define ExplorerZoxideFile G_FILE_ENTRIES[FILE_EXPLORER_ZOXIDE].mf

void TriggerExplorer(void);

#endif
