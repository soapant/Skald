#ifndef CC_REGISTRY_H
#define CC_REGISTRY_H

#include <stdbool.h>
#include "core/flapi.h"

/* Core-owned mirror of the fl_add_to_cc registry. feature_loader.c drops
 * entries belonging to a retired .so; control_center reads the same list. */
int fl_cc_registry_count(void);
const FlCcEntrySpec *fl_cc_registry_at(int index);
void fl_cc_registry_purge_owner(const char *path_a, const char *path_b);
void fl_cc_registry_reset(void);

#endif
