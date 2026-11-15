#ifndef HELPERS_MISC_H
#define HELPERS_MISC_H

#include <stdbool.h>
#include <stddef.h>

bool is_safe_process_name(const char *name);
bool lookup_ci_eq(const char *a, const char *b);
char *lookup_first_line(char *out);
bool path_next_entry(const char **cursor, char sep, char *out, size_t out_cap);

#endif
