#ifndef PLATFORM_WIDGET_H
#define PLATFORM_WIDGET_H

#include <stdbool.h>
#include "platform/render_thread.h"

typedef struct PlatformWidget PlatformWidget;

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps);
void Platform_Widget_Destroy(PlatformWidget *widget);
bool Platform_Widget_IsValid(const PlatformWidget *widget);
void Platform_Widget_SetFrameCallback(PlatformWidget *widget, Platform_RenderFrameFn fn, void *ud);
void Platform_Widget_Show(PlatformWidget *widget);
void Platform_Widget_Hide(PlatformWidget *widget);
void Platform_Widget_Move(PlatformWidget *widget, int x, int y);
int Platform_Widget_Width(const PlatformWidget *widget);
int Platform_Widget_Height(const PlatformWidget *widget);

#endif
