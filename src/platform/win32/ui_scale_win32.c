#include "platform/platform.h"
#include "config.h"

#include "core/timer.h"
float Platform_GetUIScale(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetUIScale"); return 1.0f; }
}

void Platform_ApplyUIScale(void) {
    PROFILE_START(_pt);
}

int Platform_GetUnscaledFontSize(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetUnscaledFontSize"); return FontSize; }
}

void Platform_AdjustFontSize(int delta) {
        PROFILE_START(_pt);
FontSize += delta;
    if (FontSize < 8) FontSize = 8;
    if (FontSize > 48) FontSize = 48;
}

void Platform_ResetUIScale(void) {
    PROFILE_START(_pt);
}
