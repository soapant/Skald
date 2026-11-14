#ifndef CORE_RELOAD_LIB_H
#define CORE_RELOAD_LIB_H

#include <stdbool.h>

bool ReloadLib_IsFresh(const char *src, const char *out);

bool ReloadLib_Compile(const char *src, const char *out, const char *log_tag);

char *ReloadLib_UniqueCopy(const char *src, const char *base, int seq,
                           const char *ext_suffix);

int ReloadLib_SweepStaleHotLibs(const char *dir, const char *lib_ext);

#endif
