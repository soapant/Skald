#ifndef FEATURE_STORE_H
#define FEATURE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include "helpers/helpers_text.h"

bool feature_store_feature_name(const char *source_file, char *out,
                                size_t cap);
bool feature_store_set(const char *feature, const char *key,
                       const char *value);
bool feature_store_append(const char *feature, const char *key,
                          const char *value);
StringList feature_store_get_all(const char *feature, const char *key);
char *feature_store_get_last(const char *feature, const char *key);
bool feature_store_replace_all(const char *feature, const char *key,
                               const char *const *values, int count);
bool feature_store_clear(const char *feature, const char *key);

#endif
