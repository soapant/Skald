#ifndef PLATFORM_X11_WINDOW_FACTORY_H
#define PLATFORM_X11_WINDOW_FACTORY_H

#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

#if !defined(_WIN32)
#include <sys/types.h>
pid_t GetWindowPid(Display* dpy, Window w);
void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen);
Window Linux_CreateWindowEx(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask);
Window Linux_CreatePromptWindow(Display* dpy, int x, int y, int w, int h);
Window Linux_CreateEditPopupWindow(Display* dpy, int x, int y, int w, int h);
Window Linux_CreateDisplayMessageWindow(Display* dpy, int x, int y, int w, int h);
XftDraw* Linux_CreateXftDraw(Display* dpy, Window win);
#endif

#endif
