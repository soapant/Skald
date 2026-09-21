#ifndef REMAP_MANAGER_H
#define REMAP_MANAGER_H

#include <stdbool.h>

bool remap_register_internal(const char* src, const char* dst, bool whitelist, const char** windows);
void remap_send_combo(const char* combo);
bool remap_handle_vk(int vk_code, int value);
void remap_update_phys(int vk_code, int value);
bool remap_should_swallow_linux_key(int linux_code, int value);
bool remap_should_swallow_vk(int vk_code, int value);
bool remap_has_bindings(void);
void remap_reset(void);
int remap_current_mods(void);
bool remap_is_virt_held(int vk);

#endif
