#ifndef MISC_WIN32_H
#define MISC_WIN32_H

#include "core/types.h"

void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight);
void Platform_DestroyFont(FontHandle font);
FontHandle Platform_CreateFont(int size, const char *face_name);
const char *Platform_GetRealUser(void);
const char *Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);

#endif
