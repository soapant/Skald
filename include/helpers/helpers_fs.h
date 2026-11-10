#ifndef HELPERS_FS_H
#define HELPERS_FS_H

#include <stdbool.h>
#include <stddef.h>

bool ExpandPath(const char *template_path, char *out, size_t out_size);
long File_GetSize(const char *filepath);
bool TempFile_Write(const char *name, const void *data, size_t len,
                    char *out_path, size_t out_len);
bool FeaturePath_Get(const char *feature, const char *name, char *out,
                     size_t out_len);
bool FeaturePath_DataDirExists(const char *stem);

#endif
