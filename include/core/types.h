#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifndef MAX_VISIBLE_ITEMS
#define MAX_VISIBLE_ITEMS 10000
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/types/file.h"

typedef void (*DirChangeCallback)(const char *full_path, int added);

#endif
