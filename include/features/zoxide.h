#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdbool.h>
#include "core/types.h"

#define FILE_TERMINAL_ZOXIDE 3
#define TerminalZoxideFile G_FILE_ENTRIES[FILE_TERMINAL_ZOXIDE].mf

void TriggerZoxide(void);

#endif
