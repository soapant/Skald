#ifndef RELOAD_LIB_H
#define RELOAD_LIB_H

#include <stdbool.h>
#include <stddef.h>

bool reload_lib_is_fresh(const char *src, const char *out);
bool reload_lib_content_hash(const char *path, unsigned long long *out);

bool reload_lib_compile(const char *src, const char *out, const char *log_tag);

char *reload_lib_unique_copy(const char *src, const char *base, int seq,
                             const char *ext_suffix);

int reload_lib_sweep_stale_hot_libs(const char *dir, const char *lib_ext);

void library_track_add(void *lib, const char *canon);
void library_track_remove(size_t idx);
size_t library_track_count(void);
void *library_track_lib(size_t idx);
const char *library_track_canon(size_t idx);

typedef enum {
  LIB_TRACK_NOT_FOUND,
  LIB_TRACK_SELF_CALLER,
  LIB_TRACK_REMOVED
} LibraryTrackRemoveResult;

LibraryTrackRemoveResult library_track_remove_checked(void *lib,
                                                      const char *caller_canon);

#endif
