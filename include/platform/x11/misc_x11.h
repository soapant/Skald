#ifndef MISC_X11_H
#define MISC_X11_H

#include "core/types.h"
#include <X11/Xlib.h>

bool Platform_ExpandPath(const char *template_path, char *out, size_t out_size);
void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight);
FontHandle Platform_CreateFont(int size, const char *face_name);
void Platform_DestroyFont(FontHandle font);
FontHandle Platform_CreateFontOnDisplay(Display *dpy, int size,
                                        const char *face_name);
void Platform_DismissIndefiniteMessage(void);
const char *Platform_GetSessionDisplay(void);
const char *Platform_GetSessionXAuthority(void);
const char *Platform_GetRuntimeDir(void);
const char *Platform_GetRealUser(void);
const char *Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);

#endif
