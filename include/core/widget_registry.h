#ifndef CORE_WIDGET_REGISTRY_H
#define CORE_WIDGET_REGISTRY_H

#include <stdbool.h>

void fl_widget_purge_owner(const char *path_a, const char *path_b, unsigned generation);
bool fl_widget_owner_active(const char *path);
unsigned fl_widget_bump_generation(void);
unsigned fl_widget_current_generation(void);

#endif
