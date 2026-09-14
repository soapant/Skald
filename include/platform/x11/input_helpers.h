#ifndef INPUT_HELPERS_H
#define INPUT_HELPERS_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stddef.h>

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
extern bool g_x11_ctrl_held;
extern bool g_x11_alt_held;
extern bool g_x11_shift_held;

bool should_swallow_pending(int wParam, int mods);
int Linux_KeysymToWindowsVK(KeySym keysym);
void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed);
bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed);

void EditBufferBackspace(char* buf, size_t* cursor);
bool EditBufferDeleteChar(char* buf, size_t* cursor);

void HandleCommandExecution(ListBoxInfo* lb, char* string);

#endif
