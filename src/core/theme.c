#include "core/theme.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"

#ifndef CONFIG_SRC
#define CONFIG_SRC "config/config.c"
#endif

static const struct {
  const char *name;
  size_t off;
} g_theme_slots[] = {
    {"COLOR_BG", offsetof(ThemeEntry, bg)},
    {"COLOR_TEXT", offsetof(ThemeEntry, text)},
    {"COLOR_SELECTED_TEXT", offsetof(ThemeEntry, selected_text)},
    {"COLOR_CURSOR", offsetof(ThemeEntry, cursor)},
    {"COLOR_BORDER", offsetof(ThemeEntry, border)},
};
static ColorRef *g_theme_targets[] = {
    &COLOR_BG, &COLOR_TEXT, &COLOR_SELECTED_TEXT, &COLOR_CURSOR, &COLOR_BORDER,
};

#define THEME_SLOT_COUNT (sizeof(g_theme_slots) / sizeof(g_theme_slots[0]))

void Theme_Apply(void) {
  if (!Theme || !Theme[0])
    return;
  const ThemeEntry *found = Theme_Find(Theme);
  if (!found) {
    log_warn("unknown Theme \"%s\", keeping current colors", Theme);
    return;
  }
  ColorRef saved[THEME_SLOT_COUNT];
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
    saved[i] = *g_theme_targets[i];
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
    *g_theme_targets[i] =
        *(ColorRef *)((const char *)found + g_theme_slots[i].off);
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++) {
    if (saved[i] != *g_theme_targets[i] &&
        Theme_ColorPinned(g_theme_slots[i].name))
      *g_theme_targets[i] = saved[i];
  }
}

#define THEME_MAX_PINNED 32
static char g_theme_pinned[THEME_MAX_PINNED][64];
static int g_theme_pinned_count = 0;
static bool g_theme_pinned_scanned = false;

bool Theme_PinScanFile(const char *path) {
  memset(g_theme_pinned, 0, sizeof(g_theme_pinned));
  g_theme_pinned_count = 0;
  g_theme_pinned_scanned = true;
  if (!path || !path[0])
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == ' ' || line[0] == '\t') {
      char name[64];
      if (sscanf(line, " %63[A-Z_] =", name) == 1 &&
          strncmp(name, "COLOR_", 6) == 0) {
        bool known = false;
        for (int i = 0; i < g_theme_pinned_count; i++) {
          if (strcmp(g_theme_pinned[i], name) == 0) {
            known = true;
            break;
          }
        }
        if (!known && g_theme_pinned_count < THEME_MAX_PINNED) {
          snprintf(g_theme_pinned[g_theme_pinned_count],
                   sizeof(g_theme_pinned[0]), "%s", name);
          g_theme_pinned_count++;
        }
      }
    }
  }
  fclose(f);
  return true;
}

void Theme_ResetPinned(void) {
  memset(g_theme_pinned, 0, sizeof(g_theme_pinned));
  g_theme_pinned_count = 0;
  g_theme_pinned_scanned = false;
}

bool Theme_ColorPinned(const char *name) {
  if (!name || !name[0])
    return false;
  if (!g_theme_pinned_scanned)
    Theme_PinScanFile(CONFIG_SRC);
  for (int i = 0; i < g_theme_pinned_count; i++) {
    if (strcmp(g_theme_pinned[i], name) == 0)
      return true;
  }
  return false;
}
