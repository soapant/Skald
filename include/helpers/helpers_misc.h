#ifndef HELPERS_MISC_H
#define HELPERS_MISC_H

#include <stdbool.h>

void *http_async_worker(void *arg);

bool is_safe_process_name(const char *name);

#endif
