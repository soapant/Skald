#ifndef CORE_WIDGET_REGISTRY_H
#define CORE_WIDGET_REGISTRY_H

#include <stdbool.h>

/* Internal bridge used by the feature loader on hot reload. Not part of the
   public fl_ surface: features never call these. They let the loader drop
   widgets that belong to a shared library it is about to retire, so a render
   callback can never point into unloaded code. */
void fl_widget_purge_owner(const char *path_a, const char *path_b, unsigned generation);
bool fl_widget_owner_active(const char *path);
unsigned fl_widget_bump_generation(void);
unsigned fl_widget_current_generation(void);

#endif
