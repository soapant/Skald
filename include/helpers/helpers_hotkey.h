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

bool Hotkey_AlwaysEnabled(void);
bool Hotkey_AlwaysDisabled(void);
HotkeyBinding
Hotkey_ConvertIntoBinding(char *hotkey_string,
                            void (*on_hotkey_trigger)(void),
                            bool (*optional_condition_for_hotkey)(void),
                            const char *const *optional_blacklistedwindows,
                            bool whitelist, void *user_ud);
void Hotkey_PurgeOwner(const char *path_a, const char *path_b);
void Hotkey_KeyUp(int vk_code);

#endif
