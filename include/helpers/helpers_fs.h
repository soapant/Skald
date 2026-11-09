#ifndef HELPERS_FS_H
#define HELPERS_FS_H

#include <stdbool.h>
#include <stddef.h>

bool ExpandPath(const char *template_path, char *out, size_t out_size);
bool FeaturePath_Get(const char *feature, const char *name, char *out,
                     size_t out_len);
bool FeaturePath_DataDirExists(const char *stem);

#endif
