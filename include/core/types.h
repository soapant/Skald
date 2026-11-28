#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/types/file.h"

typedef struct {
  int w;
  int h;
  unsigned char *pixels;
  void *platform;
} ImageBuffer;

typedef void (*DirChangeCallback)(const char *full_path, int added);

typedef struct {
  const char *name;
  const char *key;
  const char *default_hotkey;
  void (*cb)(void);
  void (*cb_ud)(void *);
  bool (*should_swallow)(void);
  const char *whitelist[16];
  const char *blacklist[16];
  void *ud;
} ConfigHotkeyRegister;

#endif
