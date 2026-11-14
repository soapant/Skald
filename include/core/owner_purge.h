#ifndef CORE_OWNER_PURGE_H
#define CORE_OWNER_PURGE_H

void RunEvery_PurgeOwner(const char *path_a, const char *path_b);
void PurgeOwnerSet(const char *path_a, const char *path_b,
                   unsigned widget_gen);

#endif
