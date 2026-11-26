#ifndef CONFIG_VALUES_H
#define CONFIG_VALUES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "core/types/color.h"

#include "config_defs.h"

#define X(type, name, def) extern _Atomic(type) name;
CONFIG_VALUES(X)
#undef X

#endif
