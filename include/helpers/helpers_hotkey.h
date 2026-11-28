#ifndef HELPERS_HOTKEY_H
#define HELPERS_HOTKEY_H

#include <stdbool.h>
#include "core/types.h"
#include "core/owner_registry.h"

typedef void (*HotkeyCallbackUd)(void *);
typedef bool (*HotkeyCondition)(void);

typedef struct {
  int vk;
  int modifiers;
  char **window_list;
  int window_count;
  bool whitelist;
  bool valid;
  void (*hotkey_callback)(void);
  HotkeyCallbackUd hotkey_callback_ud;
  bool (*should_swallow)(void);
  HotkeyCondition hotkey_condition;
  bool hold;
  void *user_ud;
  OwnerTag tag;
} HotkeyBinding;

#define HOTKEY_HOLD_MIN_INTERVAL_MS 10

typedef struct {
  char *f_hotkey_string;
  void (*f_on_hotkey_trigger)(void);
  bool (*f_optional_condition_for_hotkey)(void);
  const char *const *f_optional_blacklistedwindows;
  bool f_whitelist;
  void *f_user_ud;
} FnHotkeyConvertIntoBindingSpec;

HotkeyBinding
hotkey_convert_into_binding(FnHotkeyConvertIntoBindingSpec *argspec);
void hotkey_purge_owner(const char *path_a, const char *path_b);
void hotkey_key_up(int vk);

#endif
