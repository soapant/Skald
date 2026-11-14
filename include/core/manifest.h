#ifndef MANIFEST_H
#define MANIFEST_H

#include <stdbool.h>
#include <stddef.h>

#define MANIFEST_LIST_MAX 32

typedef struct {
  char name[64];
  char deps[MANIFEST_LIST_MAX][64];
  int dep_count;
  char provides[MANIFEST_LIST_MAX][96];
  int provide_count;
  bool valid;
} FeatureManifest;

void manifest_trim(char *s);
void manifest_unquote(char *s);
bool manifest_parse_string(const char *name, const char *data,
                          FeatureManifest *out);
bool manifest_parse_file(const char *path, const char *name,
                        FeatureManifest *out);

#endif
