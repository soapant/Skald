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
Window x11_create_window_ex(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask);
Window x11_create_prompt_window(Display* dpy, int x, int y, int w, int h);
Window x11_create_edit_popup_window(Display* dpy, int x, int y, int w, int h);
Window x11_create_display_message_window(Display* dpy, int x, int y, int w, int h, bool clickable,void (*render)(void));
void x11_apply_click_through(Display* dpy, Window win);
XftDraw* x11_create_xft_draw(Display* dpy, Window win);
#endif

#endif
