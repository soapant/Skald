#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/types/file.h"

typedef void (*DirChangeCallback)(const char *full_path, int added);

#endif
