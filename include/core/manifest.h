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

void Manifest_Trim(char *s);
void Manifest_Unquote(char *s);
bool Manifest_ParseString(const char *name, const char *data,
                          FeatureManifest *out);
bool Manifest_ParseFile(const char *path, const char *name,
                        FeatureManifest *out);

#endif
