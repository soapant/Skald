#ifndef CORE_FEATURE_PATH_H
#define CORE_FEATURE_PATH_H

#include <stdbool.h>
#include <stddef.h>

void feature_path_sanitize_stem(const char *src, char *out, size_t out_len);
bool feature_path_lib_basename_stem(const char *base, char *out, size_t out_len);
bool feature_path_stem_from_caller(void *caller, char *out, size_t out_len);
bool feature_path_resolve_data_path(void *caller, const char *name, char *out,
                                 size_t out_len);
bool feature_path_resolve_asset_path(void *caller, const char *name, char *out,
                                  size_t out_len);

#endif
