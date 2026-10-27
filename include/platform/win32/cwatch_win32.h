#ifndef CWATCH_WIN32_H
#define CWATCH_WIN32_H

#include "core/types.h"

bool  win32_cwatch_active(void);
char* win32_cwatch_poll(const char* root);
void  win32_cwatch_stop(void);

#endif
