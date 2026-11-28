#ifndef HELPERS_FS_H
#define HELPERS_FS_H

#include <stdbool.h>
#include <stddef.h>

#define XDG_DATA_DIRS_FALLBACK "/usr/local/share:/usr/share"

bool data_dir(char *out, size_t out_len);
void data_dir_set_override(const char *path);
bool expand_path(const char *template_path, char *out, size_t out_size);
typedef struct {
  const char *f_name;
  const void *f_data;
  size_t f_len;
  char *f_out_path;
  size_t f_out_len;
} FnTempFileWriteSpec;

bool temp_file_write(FnTempFileWriteSpec *argspec);
bool feature_path_get(const char *feature, const char *name, char *out,
                      size_t out_len);
bool feature_path_data_dir_exists(const char *stem);

bool file_copy_path(const char *src, const char *dst);
bool file_remove_path(const char *path);

#endif
