#ifndef CONFIG_TOML_H
#define CONFIG_TOML_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types/color.h"

typedef enum {
  CFG_TOML_BOOL,
  CFG_TOML_INT,
  CFG_TOML_FLOAT,
  CFG_TOML_STRING,
} CfgTomlValueType;

typedef struct {
  char section[32];
  char key[96];
  CfgTomlValueType type;
  char raw[1024];
} CfgTomlEntry;

typedef enum {
  CFG_TOML_OK = 0,
  CFG_TOML_ERR_IO,
  CFG_TOML_ERR_PARSE,
} CfgTomlStatus;

typedef bool (*CfgTomlVisitFn)(const CfgTomlEntry *entry, void *user);

CfgTomlStatus cfg_toml_parse(const char *path, CfgTomlVisitFn visit,
                             void *user);

bool cfg_toml_decode_string(const char *raw, char *out, size_t out_cap);

bool cfg_toml_encode_string(const char *src, char *out, size_t out_cap);

bool cfg_toml_color_ref(const char *body, ColorRef *out);

#endif
