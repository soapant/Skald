#include <stdbool.h>

#ifndef HELPERS_KEY_H
#define HELPERS_KEY_H

#include "core/types/keys.h"

bool key_is_ctrl_pressed(void);
bool key_is_shift_pressed(void);
bool key_is_alt_pressed(void);
bool key_is_super_pressed(void);

typedef bool (*KeyInterceptorFn)(const PopupKeyEvent *key);

void key_register_interceptor(KeyInterceptorFn fn);
void key_purge_interceptor_owner(const char *path_a, const char *path_b);
bool key_invoke_interceptor(const PopupKeyEvent *key);

#endif
