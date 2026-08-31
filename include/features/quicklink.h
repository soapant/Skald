#ifndef QUICKLINK_H
#define QUICKLINK_H
#include <stdbool.h>
void open_browser(const char *url);
bool CheckQuickLinkHotkeyBindings(int pressed_vk, int modifiers);
void quicklink_init(void);
#endif
