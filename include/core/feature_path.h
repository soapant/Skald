#ifndef FEATURE_PATH_H
#define FEATURE_PATH_H

#include <stdbool.h>
#include <stddef.h>

void feature_path_sanitize_stem(const char *src, char *out, size_t out_len);
bool feature_path_lib_basename_stem(const char *base, char *out,
                                    size_t out_len);
bool feature_path_stem_from_caller(void *caller, char *out, size_t out_len);
bool feature_path_stem_from_owner(const char *owner, char *out, size_t out_len);
bool feature_path_resolve_data_path(void *caller, const char *name, char *out,
                                    size_t out_len);
bool feature_path_resolve_asset_path(void *caller, const char *name, char *out,
                                     size_t out_len);
bool feature_path_resolve_asset_for_owner(const char *owner, const char *name,
                                          char *out, size_t out_len);
const char *feature_path_resolve_asset_or_path(void *caller, const char *path,
                                               char *asset, size_t asset_cap);

#endif
