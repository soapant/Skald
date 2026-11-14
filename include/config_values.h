#ifndef CONFIG_VALUES_H
#define CONFIG_VALUES_H

#include <stddef.h>
#include <stdbool.h>
#include "core/types/color.h"

#include "config_defs.h"

#define X(type, name, def) extern type name;
SKALD_CONFIG_VALUES(X)
#undef X

#endif
