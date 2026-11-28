#ifndef CONFIG_H
#define CONFIG_H
#include <stddef.h>
#include "core/types.h"
#include "config_values.h"

#define CONFIG_SRC "config/config.toml"
#define CONFIG_COLOR_PREFIX "COLOR_"
#define CONFIG_NAME_STRING(x) #x

static inline const char *config_color_short(const char *name) {
  return name + sizeof(CONFIG_COLOR_PREFIX) - 1;
}

#define EDIT_TEXT_KEY 'E'
#define HELP_SCREEN_KEY 'H'
#define COPY_SELECTION_KEY 'Y'

void config_resolve_default_apps(void);
bool config_is_auto_resolved_app(const char *name);
bool config_load(void);
bool config_load_path(const char *path);
bool config_reload(void);
void config_own_string(const char *name, char *s);
void config_unload(void);
bool config_save(void);
bool config_save_path(const char *path);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
