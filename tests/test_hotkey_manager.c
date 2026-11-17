#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/hotkey_manager.h"
#include "core/types.h"
#include "helpers/helpers_hotkey.h"

#define TEST_VK_A 0xF0
#define TEST_VK_B 0xF1
#define TEST_VK_C 0xF2
#define TEST_VK_D 0xF3

static int ud_calls = 0;
static void *ud_seen = NULL;
static void ud_cb(void *ud) {
  ud_calls++;
  ud_seen = ud;
}

static int fn_calls = 0;
static void fn_cb(void) { fn_calls++; }

static HotkeyBinding make_binding(int vk, int mods, HotkeyCallbackUd cb_ud,
                                  void *ud, HotkeyCallback cb) {
  HotkeyBinding b = {0};
  b.vk_code = vk;
  b.modifiers = mods;
  b.hotkey_callback = cb;
  b.hotkey_callback_ud = cb_ud;
  b.user_ud = ud;
  return b;
}

static void reset_counters(void) {
  ud_calls = 0;
  ud_seen = NULL;
  fn_calls = 0;
}

TEST(register_and_drain_ud_callback) {
  reset_counters();
  int marker = 0x1234;
  HotkeyBinding b = make_binding(TEST_VK_A, 0, ud_cb, &marker, NULL);
  ASSERT_TRUE(hotkey_register_internal(b));
  ASSERT_TRUE(hotkey_process(TEST_VK_A, 0));
  hotkey_drain_pending();
  ASSERT_EQ_INT(ud_calls, 1);
  ASSERT_EQ_INT(ud_seen == &marker, 1);
  hotkey_drain_pending();
  ASSERT_EQ_INT(ud_calls, 1);
}

TEST(register_and_drain_legacy_callback) {
  reset_counters();
  HotkeyBinding b = make_binding(TEST_VK_B, 0, NULL, NULL, fn_cb);
  ASSERT_TRUE(hotkey_register_internal(b));
  ASSERT_TRUE(hotkey_process(TEST_VK_B, 0));
  hotkey_drain_pending();
  ASSERT_EQ_INT(fn_calls, 1);
}

TEST(unregister_removes_binding) {
  reset_counters();
  HotkeyBinding b = make_binding(TEST_VK_C, 0, ud_cb, &reset_counters, NULL);
  ASSERT_TRUE(hotkey_register_internal(b));
  ASSERT_TRUE(hotkey_is_taken(TEST_VK_C, 0));
  ASSERT_TRUE(hotkey_unregister_internal(TEST_VK_C, 0));
  ASSERT_FALSE(hotkey_is_taken(TEST_VK_C, 0));
  hotkey_drain_pending();
  ASSERT_EQ_INT(ud_calls, 0);
}

TEST(purge_owner_removes_binding_and_pending) {
  reset_counters();
  int marker = 0xABCD;
  HotkeyBinding b = make_binding(TEST_VK_D, 0, ud_cb, &marker, NULL);
  ASSERT_TRUE(hotkey_register_internal(b));
  int found = -1;
  for (int i = 0; i < hotkey_count(); i++) {
    const HotkeyBinding *x = hotkey_get(i);
    if (x && x->vk_code == TEST_VK_D)
      found = i;
  }
  ASSERT_TRUE(found >= 0);
  ASSERT_TRUE(hotkey_process(TEST_VK_D, 0));
  hotkey_purge_owner(hotkey_get(found)->owner, NULL);
  ASSERT_FALSE(hotkey_is_taken(TEST_VK_D, 0));
  reset_counters();
  hotkey_drain_pending();
  ASSERT_EQ_INT(ud_calls, 0);
}

TEST(keyboard_binding_detection_ignores_mouse) {
  HotkeyBinding mouse_binding = make_binding(0xE3, 0, ud_cb, NULL, NULL);
  ASSERT_TRUE(hotkey_register_internal(mouse_binding));
  ASSERT_FALSE(hotkey_has_keyboard_binding());
  HotkeyBinding kb_binding = make_binding('Z', 0, ud_cb, NULL, fn_cb);
  ASSERT_TRUE(hotkey_register_internal(kb_binding));
  ASSERT_TRUE(hotkey_has_keyboard_binding());
  ASSERT_TRUE(hotkey_unregister_internal('Z', 0));
  ASSERT_TRUE(hotkey_unregister_internal(0xE3, 0));
  ASSERT_FALSE(hotkey_has_keyboard_binding());
}

TEST(suspended_hotkeys_do_not_dispatch) {
  reset_counters();
  hotkey_set_suspended(true);
  HotkeyBinding b = make_binding(0xF4, 0, ud_cb, &reset_counters, NULL);
  ASSERT_TRUE(hotkey_register_internal(b));
  hotkey_process(0xF4, 0);
  hotkey_drain_pending();
  ASSERT_EQ_INT(ud_calls, 0);
  hotkey_set_suspended(false);
  hotkey_unregister_internal(0xF4, 0);
}

int main(void) {
  RUN(register_and_drain_ud_callback);
  RUN(register_and_drain_legacy_callback);
  RUN(unregister_removes_binding);
  RUN(purge_owner_removes_binding_and_pending);
  RUN(keyboard_binding_detection_ignores_mouse);
  RUN(suspended_hotkeys_do_not_dispatch);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
