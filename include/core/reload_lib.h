#ifndef RELOAD_LIB_H
#define RELOAD_LIB_H

#include <stdbool.h>

bool reload_lib_is_fresh(const char *src, const char *out);

bool reload_lib_compile(const char *src, const char *out, const char *log_tag);

char *reload_lib_unique_copy(const char *src, const char *base, int seq,
                             const char *ext_suffix);

int reload_lib_sweep_stale_hot_libs(const char *dir, const char *lib_ext);

#endif
