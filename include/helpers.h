#ifndef HELPERS_H
#define HELPERS_H

// Compatibility shim - prefer direct includes of helpers/*.h
// Kept lean: drawing/window_factory no longer pulled transitively (include directly where needed).
#include <stddef.h>
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);


#endif
