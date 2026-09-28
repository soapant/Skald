#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_VISIBLE_ITEMS 10000
#define MAX_SEQUENCE_LEN 8
#define SEQUENCE_TIMEOUT_MS 200

#define MAX_PATH 260

#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/types/file.h"
#include "core/types/misc.h"

typedef void (*DirChangeCallback)(const char* full_path, int added);

#endif
