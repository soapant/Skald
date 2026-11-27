#include "core/theme.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdatomic.h>

#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"
#include "core/config_toml.h"
#include "deps/stb_ds.h"

#ifndef CONFIG_SRC
#define CONFIG_SRC "config/config.toml"
#endif

static const struct {
  const char *name;
  size_t off;
} theme_slots[] = {
    {"COLOR_BG", offsetof(ThemeEntry, bg)},
    {"COLOR_TEXT", offsetof(ThemeEntry, text)},
    {"COLOR_SELECTED_TEXT", offsetof(ThemeEntry, selected_text)},
    {"COLOR_CURSOR", offsetof(ThemeEntry, cursor)},
    {"COLOR_BORDER", offsetof(ThemeEntry, border)},
};

static _Atomic(ColorRef) *theme_targets[] = {
    &COLOR_BG, &COLOR_TEXT, &COLOR_SELECTED_TEXT, &COLOR_CURSOR, &COLOR_BORDER,
};

#define THEME_SLOT_COUNT (sizeof(theme_slots) / sizeof(theme_slots[0]))

typedef struct {
  char name[64];
  ColorRef value;
} PinnedColor;

static PinnedColor *theme_pinned;
#define theme_pinned_count arrlenu(theme_pinned)

static bool slot_offset(const char *name, size_t *off) {
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++) {
    if (strcmp(theme_slots[i].name, name) == 0) {
      if (off)
        *off = theme_slots[i].off;
      return true;
    }
  }
  return false;
}

static bool matches_any_theme(size_t off, ColorRef value) {
  size_t n = theme_count();
  for (size_t i = 0; i < n; i++) {
    const ThemeEntry *e = theme_entry_at(i);
    if (e && *(const ColorRef *)((const char *)e + off) == value)
      return true;
  }
  return false;
}

static bool pin_add(const char *name, ColorRef value) {
  for (size_t i = 0; i < theme_pinned_count; i++) {
    if (strcmp(theme_pinned[i].name, name) == 0)
      return true;
  }
  PinnedColor *p = arraddnptr(theme_pinned, 1);
  snprintf(p->name, sizeof(p->name), "%s", name);
  p->value = value;
  return true;
}

static bool pin_lookup(const char *name, ColorRef *out) {
  for (size_t i = 0; i < theme_pinned_count; i++) {
    if (strcmp(theme_pinned[i].name, name) == 0) {
      if (out)
        *out = theme_pinned[i].value;
      return true;
    }
  }
  return false;
}

static bool theme_pin_visit(const ConfigTomlEntry *e, void *user) {
  (void)user;
  if (strcmp(e->section, "color") != 0)
    return true;
  bool known = false;
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
    if (strcmp(theme_slots[i].name + 6, e->key) == 0) {
      known = true;
      break;
    }
  if (!known)
    return true;
  char body[1024];
  if (!config_toml_decode_string(e->raw, body, sizeof(body)))
    return true;
  ColorRef v;
  if (!config_toml_color_ref(body, &v))
    return true;
  char name[64];
  const int prefix_len = (int)(sizeof("COLOR_") - 1);
  const int key_cap = (int)sizeof(name) - prefix_len - 1;
  int written = snprintf(name, sizeof(name), "COLOR_%.*s", key_cap, e->key);
  if (written < 0 || written >= (int)sizeof(name))
    return true;
  pin_add(name, v);
  return true;
}

bool theme_pin_scan_file(const char *path) {
  arrsetlen(theme_pinned, 0);
  if (!path || !path[0])
    return false;
  ConfigTomlStatus st = config_toml_parse(path, theme_pin_visit, NULL);
  return st == CONFIG_TOML_OK;
}

void theme_reset_pinned(void) { arrsetlen(theme_pinned, 0); }

bool theme_color_pinned(const char *name) {
  if (!name || !name[0])
    return false;
  size_t off;
  ColorRef value;
  if (!slot_offset(name, &off) || !pin_lookup(name, &value))
    return false;
  return !matches_any_theme(off, value);
}

void theme_apply(void) {
  const char *theme_name = atomic_load_explicit(&Theme, memory_order_acquire);
  if (!theme_name || !theme_name[0])
    return;
  const ThemeEntry *found = theme_find(theme_name);
  if (!found) {
    log_warn("unknown Theme \"%s\", keeping current colors", theme_name);
    return;
  }
  theme_pin_scan_file(CONFIG_SRC);
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++) {
    ColorRef theme_color =
        *(ColorRef *)((const char *)found + theme_slots[i].off);
    ColorRef pinned;
    bool user_override = false;
    if (pin_lookup(theme_slots[i].name, &pinned) &&
        !matches_any_theme(theme_slots[i].off, pinned))
      user_override = true;
    atomic_store_explicit(theme_targets[i],
                          user_override ? pinned : theme_color,
                          memory_order_release);
  }
}
