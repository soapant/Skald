#ifndef CONFIG_SECTION_H
#define CONFIG_SECTION_H

#include <stdbool.h>

#include "core/config_toml.h"

bool config_section_set(const char *path, const char *section, const char *name,
                        const char *value);
bool config_section_clear(const char *path, const char *section,
                          const char *name);

ConfigTomlStatus config_section_set_ex(const char *path, const char *section,
                                       const char *name, const char *value,
                                       ConfigTomlError *err);
ConfigTomlStatus config_section_clear_ex(const char *path, const char *section,
                                         const char *name,
                                         ConfigTomlError *err);

#endif
