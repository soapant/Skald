#ifndef CONFIG_H
#define CONFIG_H
#include <stddef.h>
#include "core/types.h"
#include "config_values.h"

#define EDIT_TEXT_KEY 'E'
#define HELP_SCREEN_KEY 'H'
#define COPY_SELECTION_KEY 'Y'

void config_resolve_default_apps(void);
bool config_is_auto_resolved_app(const char *name);
bool config_load(void);
bool config_reload(void);
void config_unload(void);
void config_save(void);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
