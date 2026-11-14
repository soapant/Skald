#ifndef HELPERS_MISC_H
#define HELPERS_MISC_H

#include <stdbool.h>

void *HttpAsyncWorker(void *arg);

bool IsSafeProcessName(const char *name);

#endif
