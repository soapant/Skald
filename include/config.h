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
size_t quicklink_count(void);
bool quicklink_at(size_t index, char *name_out, size_t name_cap,
                  char *target_out, size_t target_cap);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
