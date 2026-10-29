#ifndef CC_REGISTRY_H
#define CC_REGISTRY_H

#include "core/flapi.h"

// Re-declarations for now, will remove from flapi.h later
bool add_to_cc_impl(const FlCcEntrySpec *spec);
int cc_entry_count(void);
const FlCcEntrySpec *cc_entry_at(int index);
void cc_registry_purge_owner(const char *path_a, const char *path_b);

#endif
