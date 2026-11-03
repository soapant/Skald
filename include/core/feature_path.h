#ifndef CORE_FEATURE_PATH_H
#define CORE_FEATURE_PATH_H

#include <stdbool.h>
#include <stddef.h>

void FeaturePath_SanitizeStem(const char *src, char *out, size_t out_len);
bool FeaturePath_LibBasenameStem(const char *base, char *out, size_t out_len);
bool FeaturePath_StemFromCaller(void *caller, char *out, size_t out_len);
bool FeaturePath_ResolveDataPath(void *caller, const char *name, char *out,
                                 size_t out_len);
bool FeaturePath_ResolveAssetPath(void *caller, const char *name, char *out,
                                  size_t out_len);

#endif
