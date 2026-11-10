#include <stdbool.h>

#ifndef HELPERS_KEY_H
#define HELPERS_KEY_H

#include "core/types/keys.h"

bool Key_IsCtrlPressed(void);
bool Key_IsShiftPressed(void);
bool Key_IsAltPressed(void);
bool Key_IsSuperPressed(void);

typedef bool (*KeyInterceptorFn)(const PopupKeyEvent *key);

void Key_RegisterInterceptor(KeyInterceptorFn fn);
void Key_PurgeInterceptorOwner(const char *path_a, const char *path_b);
bool Key_InvokeInterceptor(const PopupKeyEvent *key);

#endif
