#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include "core/modal.h"
#include "core/types.h"
#include "core/hashmap.h"
#include "config.h"
#include "core/platform.h"

// Modular split - includes for structure, helpers.h remains compatibility shim
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#if !defined(_WIN32)
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"
#endif

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);


#endif
