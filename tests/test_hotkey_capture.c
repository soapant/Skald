#include <string.h>

#include "test.h"

#include "core/hotkey_capture.h"
#include "core/types/keys.h"
#include "helpers/helpers_input_state.h"

static void capture_press(int vk) { input_state_update(vk, true, 0, 0); }
static void capture_release(int vk) { input_state_update(vk, false, 0, 0); }

TEST(capture_begin_end) {
  hotkey_capture_end();
  ASSERT_FALSE(hotkey_capture_active());

  hotkey_capture_begin();
  ASSERT_TRUE(hotkey_capture_active());
  ASSERT_EQ_STR(hotkey_capture_preview(), "");
  ASSERT_EQ_STR(hotkey_capture_last(), "");

  hotkey_capture_end();
  ASSERT_FALSE(hotkey_capture_active());
}

TEST(capture_poll_combo) {
  input_state_clear();
  hotkey_capture_begin();

  capture_press(VK_CONTROL);
  capture_press(VK_SHIFT);
  capture_press('A');

  char out[64] = "";
  ASSERT_TRUE(hotkey_capture_poll(out, sizeof(out)));
  ASSERT_EQ_STR(out, "Ctrl+Shift+A");
  ASSERT_EQ_STR(hotkey_capture_last(), "Ctrl+Shift+A");
  ASSERT_EQ_STR(hotkey_capture_preview(), "Ctrl+Shift+A");
  ASSERT_FALSE(hotkey_capture_poll(out, sizeof(out)));

  capture_release('A');
  capture_release(VK_SHIFT);
  capture_release(VK_CONTROL);
  hotkey_capture_end();
}

TEST(capture_poll_modifier_preview) {
  input_state_clear();
  hotkey_capture_begin();

  capture_press(VK_CONTROL);
  capture_press(VK_MENU);

  char out[64] = "";
  ASSERT_FALSE(hotkey_capture_poll(out, sizeof(out)));
  ASSERT_EQ_STR(hotkey_capture_preview(), "Ctrl+Alt");
  ASSERT_EQ_STR(out, "");

  capture_release(VK_MENU);
  capture_release(VK_CONTROL);
  hotkey_capture_end();
}

TEST(capture_poll_bare_key) {
  input_state_clear();
  hotkey_capture_begin();

  capture_press('B');

  char out[64] = "";
  ASSERT_FALSE(hotkey_capture_poll(out, sizeof(out)));
  ASSERT_EQ_STR(out, "");
  ASSERT_EQ_STR(hotkey_capture_last(), "");

  capture_release('B');
  hotkey_capture_end();
}

int main(void) {
  RUN(capture_begin_end);
  RUN(capture_poll_combo);
  RUN(capture_poll_modifier_preview);
  RUN(capture_poll_bare_key);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
