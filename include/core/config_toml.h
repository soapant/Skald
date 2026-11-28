#ifndef CONFIG_TOML_H
#define CONFIG_TOML_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types/color.h"

typedef enum {
  CONFIG_TOML_BOOL,
  CONFIG_TOML_INT,
  CONFIG_TOML_FLOAT,
  CONFIG_TOML_STRING,
} ConfigTomlValueType;

#define CONFIG_TOML_SECTION_MAX 96

typedef struct {
  char section[CONFIG_TOML_SECTION_MAX];
  char key[384];
  ConfigTomlValueType type;
  char raw[1024];
} ConfigTomlEntry;

typedef enum {
  CONFIG_TOML_OK = 0,
  CONFIG_TOML_ERR_IO,
  CONFIG_TOML_ERR_PARSE,
  CONFIG_TOML_ERR_ARG,
  CONFIG_TOML_ERR_REJECTED,
} ConfigTomlStatus;

typedef struct {
  ConfigTomlStatus status;
  int line;
  char message[256];
} ConfigTomlError;

void config_toml_error_clear(ConfigTomlError *err);
ConfigTomlStatus config_toml_error_set(ConfigTomlError *err,
                                       ConfigTomlStatus status, int line,
                                       const char *message);
const char *config_toml_status_name(ConfigTomlStatus status);

typedef bool (*ConfigTomlVisitFn)(const ConfigTomlEntry *entry, void *user);

ConfigTomlStatus config_toml_parse(const char *path, ConfigTomlVisitFn visit,
                                   void *user);
ConfigTomlStatus config_toml_parse_ex(const char *path, ConfigTomlVisitFn visit,
                                      void *user, ConfigTomlError *err);

bool config_toml_decode_string(const char *raw, char *out, size_t out_cap);

bool config_toml_encode_string(const char *src, char *out, size_t out_cap);

bool config_toml_color_ref(const char *body, ColorRef *out);

#endif
