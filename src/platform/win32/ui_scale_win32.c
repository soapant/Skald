#include "core/platform.h"
#include "config.h"

float Platform_GetUIScale(void) {
    return 1.0f;
}

void Platform_ApplyUIScale(void) {
}

int Platform_GetUnscaledFontSize(void) {
    return FontSize;
}

void Platform_AdjustFontSize(int delta) {
    FontSize += delta;
    if (FontSize < 8) FontSize = 8;
    if (FontSize > 48) FontSize = 48;
}

void Platform_ResetUIScale(void) {
}
