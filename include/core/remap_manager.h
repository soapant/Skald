#ifndef REMAP_MANAGER_H
#define REMAP_MANAGER_H

#include <stdbool.h>

#define REMAP_PARSE_MAX_VKS 8

bool remap_parse_triggers(const char *str, int *out_mods, int *out_vks,
                          int *out_count);

typedef struct {
  const char *f_src;
  const char *f_dst;
  bool f_whitelist;
  const char *const *f_windows;
  void *f_caller;
} RemapRegisterInternal;

bool remap_register_internal(RemapRegisterInternal *argspec);
void remap_purge_owner(const char *path_a, const char *path_b);
void remap_send_combo(const char *combo);
bool remap_handle_vk(int vk, int value);
void remap_update_phys(int vk, int value);
bool remap_should_swallow_linux_key(int linux_code, int value);
bool remap_should_swallow_vk(int vk, int value);
bool remap_has_bindings(void);
void remap_set_emit_hook(bool (*fn)(int vk, int down));
void remap_reset(void);
int remap_current_mods(void);
bool remap_is_virt_held(int vk);
bool remap_check_strict_order(int trigger_vk, int mods);

#endif

