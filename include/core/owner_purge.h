#ifndef CORE_OWNER_PURGE_H
#define CORE_OWNER_PURGE_H

void run_every_purge_owner(const char *path_a, const char *path_b);
void purge_owner_set(const char *path_a, const char *path_b, unsigned widget_gen);

#endif
