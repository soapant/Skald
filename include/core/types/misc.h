#ifndef TYPES_MISC_H
#define TYPES_MISC_H

#include <stdint.h>
#include "core/types.h"

typedef struct {
  char *initial_text;
  char *result;
  void *hFont;
  int alpha;
} EditPopupData;

typedef enum {
  DEFAULT_BROWSER,
  DEFAULT_TERMINAL,
  DEFAULT_SHELL,
  DEFAULT_APP_COUNT
} DefaultAppKind;

#endif
