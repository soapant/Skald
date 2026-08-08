#ifndef YAZI_ZOXIDE_H
#define YAZI_ZOXIDE_H

#include <windows.h>
#include <stdbool.h>
#include "core/types.h"

#define FILE_YAZI_ZOXIDE 1
#define YaziZoxideFile G_FILE_ENTRIES[FILE_YAZI_ZOXIDE].mf

void TriggerYaziZoxide(void);

#endif
