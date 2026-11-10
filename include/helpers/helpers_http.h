#ifndef HELPERS_HTTP_H
#define HELPERS_HTTP_H

#include <stdbool.h>
#include "platform/platform.h"

void Http_GetAsync(const char *url, PlatformHttpCallback cb, void *userdata);
bool HttpBackend_ThreadRun(void *(*fn)(void *), void *ud);

#endif
