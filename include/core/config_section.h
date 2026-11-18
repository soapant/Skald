#ifndef CONFIG_SECTION_H
#define CONFIG_SECTION_H

#include <stdbool.h>

bool config_section_set(const char *path, const char *section,
                        const char *name, const char *value);
bool config_section_clear(const char *path, const char *section,
                          const char *name);

#endif
