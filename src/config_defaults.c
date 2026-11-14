
#include <stdbool.h>

#include "config_values.h"
#include "core/types/color.h"
#include "config_defs.h"

#define X(type, name, def) type name = def;
SKALD_CONFIG_VALUES(X)
#undef X
