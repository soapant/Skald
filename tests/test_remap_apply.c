#include <string.h>

#include "test.h"

#include "core/remap_manager.h"
#include "core/types/keys.h"
#include "helpers/helpers_owner.h"

#define MAX_EMITS 64

static int EMIT_COUNT;
static int EMIT_VKS[MAX_EMITS];
static int EMIT_DOWN[MAX_EMITS];

static bool fake_emit(int vk, int down) {
  if (EMIT_COUNT < MAX_EMITS) {
    EMIT_VKS[EMIT_COUNT] = vk;
    EMIT_DOWN[EMIT_COUNT] = down;
    EMIT_COUNT++;
  }
  return true;
}

static void remap_teardown(void) {
  remap_reset();
  remap_set_emit_hook(NULL);
  EMIT_COUNT = 0;
  memset(EMIT_VKS, 0, sizeof(EMIT_VKS));
  memset(EMIT_DOWN, 0, sizeof(EMIT_DOWN));
}

static bool rejecting_emit(int vk, int down) {
  (void)vk;
  (void)down;
  return false;
}

static bool saw_emit(int vk, int down) {
  for (int i = 0; i < EMIT_COUNT; i++)
    if (EMIT_VKS[i] == vk && EMIT_DOWN[i] == down)
      return true;
  return false;
}

TEST(binding_fires_destination_and_swallows_trigger) {
  TEST_TEARDOWN(remap_teardown);
  remap_set_emit_hook(fake_emit);
  ASSERT_TRUE(remap_register_internal(&(FnRemapRegisterInternalSpec){.f_src = "Ctrl+A", .f_dst = "B", .f_whitelist = false, .f_windows = NULL, .f_caller = (void *)test_binding_fires_destination_and_swallows_trigger}));
  ASSERT_TRUE(remap_has_bindings());
  remap_update_phys(VK_LCONTROL, 1);
  ASSERT_TRUE(remap_handle_vk('A', 1));
  ASSERT_TRUE(saw_emit('B', 1));
  ASSERT_FALSE(remap_is_virt_held(VK_LCONTROL));
  remap_handle_vk('A', 0);
  ASSERT_FALSE(remap_is_virt_held('B'));
  ASSERT_TRUE(saw_emit('B', 0));
}

TEST(modifier_state_tracks_physical_keys) {
  TEST_TEARDOWN(remap_teardown);
  remap_set_emit_hook(fake_emit);
  ASSERT_EQ_INT(remap_current_mods() & HOTKEY_MOD_CTRL, 0);
  remap_update_phys(VK_LCONTROL, 1);
  ASSERT_TRUE((remap_current_mods() & HOTKEY_MOD_CTRL) != 0);
  remap_update_phys(VK_LCONTROL, 0);
  ASSERT_EQ_INT(remap_current_mods() & HOTKEY_MOD_CTRL, 0);
}

TEST(strict_order_rejects_late_modifier) {
  TEST_TEARDOWN(remap_teardown);
  remap_set_emit_hook(fake_emit);
  remap_update_phys('A', 1);
  ASSERT_TRUE(remap_check_strict_order('A', HOTKEY_MOD_CTRL));
  remap_update_phys(VK_LCONTROL, 1);
  ASSERT_FALSE(remap_check_strict_order('A', HOTKEY_MOD_CTRL));
}

TEST(purge_owner_removes_binding) {
  TEST_TEARDOWN(remap_teardown);
  char owner[OWNER_PATH_MAX];
  owner_resolve_for_func((void *)test_purge_owner_removes_binding, owner,
                         sizeof(owner));
  ASSERT_TRUE(remap_register_internal(&(FnRemapRegisterInternalSpec){.f_src = "Ctrl+K", .f_dst = "J", .f_whitelist = false, .f_windows = NULL, .f_caller = (void *)test_purge_owner_removes_binding}));
  ASSERT_TRUE(remap_has_bindings());
  if (owner[0]) {
    remap_purge_owner(owner, NULL);
    ASSERT_FALSE(remap_has_bindings());
  }
}

TEST(emit_hook_failure_keeps_virtual_key_released) {
  TEST_TEARDOWN(remap_teardown);
  remap_set_emit_hook(rejecting_emit);
  ASSERT_TRUE(remap_register_internal(&(FnRemapRegisterInternalSpec){.f_src = "Ctrl+M", .f_dst = "N", .f_whitelist = false, .f_windows = NULL, .f_caller = (void *)test_emit_hook_failure_keeps_virtual_key_released}));
  remap_update_phys(VK_LCONTROL, 1);
  remap_handle_vk('M', 1);
  ASSERT_FALSE(remap_is_virt_held('N'));
}

int main(void) {
  RUN(binding_fires_destination_and_swallows_trigger);
  RUN(modifier_state_tracks_physical_keys);
  RUN(strict_order_rejects_late_modifier);
  RUN(purge_owner_removes_binding);
  RUN(emit_hook_failure_keeps_virtual_key_released);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
