#ifndef CORE_HTTP_H
#define CORE_HTTP_H

#include <stddef.h>
#include <stdbool.h>

#include "platform/platform.h"

char *http_get(const char *url);
char *http_get_limited(const char *url, size_t max_bytes, size_t *out_len);
char *http_post(const char *url, const char *content_type, const char *body);
bool http_download(const char *url, const char *save_path);
void http_get_async(const char *url, PlatformHttpCallback cb, void *userdata);

#endif
