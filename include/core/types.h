#ifndef TYPES_H
#define TYPES_H

#if defined(_WIN32)
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_VISIBLE_ITEMS 10000
#define MAX_SEQUENCE_LEN 8
#define SEQUENCE_TIMEOUT_MS 500

#if !defined(MAX_PATH)
#define MAX_PATH 260
#endif

#if !defined(_strdup) && !defined(_WIN32)
#define _strdup strdup
#endif

#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/types/file.h"
#include "core/types/hotkey.h"
#include "core/types/misc.h"

#endif
