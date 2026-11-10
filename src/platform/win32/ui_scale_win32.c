#include "config_values.h"
#include "platform/platform.h"

float Platform_GetUIScale(void) { return 1.0f; }

void Platform_ApplyUIScale(void) {}

int Platform_GetUnscaledFontSize(void) { return FontSize; }

void Platform_ResetUIScale(void) {}
