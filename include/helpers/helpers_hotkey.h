#ifndef HELPERS_HOTKEY_H
#define HELPERS_HOTKEY_H

#include <stdbool.h>
#include "core/types.h"

typedef void (*HotkeyCallback)(void);
typedef void (*HotkeyCallbackUd)(void *);
typedef bool (*HotkeyCondition)(void);

typedef struct {
  int vk_code;
  int modifiers;
  char **blacklist;
  int blacklist_count;
  bool whitelist;
  HotkeyCallback hotkey_callback;
  HotkeyCallbackUd hotkey_callback_ud;
  bool (*should_swallow)(void);
  HotkeyCondition hotkey_condition;
  bool hold;
  void *user_ud;
  char owner[512];
} HotkeyBinding;

#define HOTKEY_HOLD_MIN_INTERVAL_MS 10

HotkeyBinding
hotkey_convert_into_binding(char *hotkey_string,
                            void (*on_hotkey_trigger)(void),
                            bool (*optional_condition_for_hotkey)(void),
                            const char *const *optional_blacklistedwindows,
                            bool whitelist, void *user_ud);
void hotkey_purge_owner(const char *path_a, const char *path_b);
void hotkey_key_up(int vk_code);

#endif
