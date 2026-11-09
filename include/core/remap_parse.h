#ifndef REMAP_PARSE_H
#define REMAP_PARSE_H

#include <stdbool.h>

#define REMAP_PARSE_MAX_VKS 8

bool Remap_ParseTriggers(const char *str, int *out_mods, int *out_vks,
                         int *out_count);

#endif
