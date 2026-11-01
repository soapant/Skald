#ifndef CORE_WIDGET_REGISTRY_H
#define CORE_WIDGET_REGISTRY_H

#include <stdbool.h>
#include "core/flapi.h"

void widget_purge_owner(const char *path_a, const char *path_b,
                        unsigned generation);
bool widget_owner_active(const char *path);
unsigned widget_bump_generation(void);
unsigned widget_current_generation(void);

FlWidget *widget_create(const FlWidgetSpec *spec);
bool widget_valid(FlWidget *widget);
FlPos widget_get_pos(const char *name);
void widget_show(FlWidget *widget);
void widget_hide(FlWidget *widget);
void widget_set_render(FlWidget *widget, FlWidgetRenderFn render);

#endif
