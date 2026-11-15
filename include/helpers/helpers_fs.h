#ifndef HELPERS_FS_H
#define HELPERS_FS_H

#include <stdbool.h>
#include <stddef.h>

#define XDG_DATA_DIRS_FALLBACK "/usr/local/share:/usr/share"

bool data_dir(char *out, size_t out_len);
bool expand_path(const char *template_path, char *out, size_t out_size);
bool temp_file_write(const char *name, const void *data, size_t len,
                    char *out_path, size_t out_len);
bool feature_path_get(const char *feature, const char *name, char *out,
                     size_t out_len);
bool feature_path_data_dir_exists(const char *stem);

bool file_copy_path(const char *src, const char *dst);

#endif
