#ifndef CONFIG_H
#define CONFIG_H
#include <stddef.h>
#include "core/types.h"
#include "config_values.h"

#define EDIT_TEXT_KEY 'E'
#define HELP_SCREEN_KEY 'H'
#define COPY_SELECTION_KEY 'Y'
#define COMMAND_MODE_KEY VK_OEM_1

#define MAX_FILE_LINES 10000
#define MAX_FILE_LINE_LENGTH 1024

void Config_ResolveDefaultApps(void);
bool Config_IsAutoResolvedApp(const char *name);
bool Config_Load(void);
bool Config_Reload(void);
void Config_Unload(void);
void Config_Save(void);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
