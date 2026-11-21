#ifndef HOTKEY_CAPTURE_H
#define HOTKEY_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>

void hotkey_capture_begin(void);
bool hotkey_capture_poll(char *out, size_t cap);
const char *hotkey_capture_preview(void);
const char *hotkey_capture_last(void);
bool hotkey_capture_active(void);
void hotkey_capture_end(void);

#endif
