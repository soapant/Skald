#include "core/hotkey_text.h"
#include "core/timer.h"
#include "core/types/keys.h"
#include <stdio.h>
#include <sys/time.h>

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool HotkeyText_IsPrintable(const PopupKeyEvent *key) {

  if (!key || key->text_len <= 0)
    return false;
  for (int i = 0; i < key->text_len; i++) {
    unsigned char c = (unsigned char)key->text[i];
    if (c < 0x20 || c == 0x7F)
      return false;
  }

  return true;
}

bool HotkeyText_ShouldSwallow(const PopupKeyEvent *key) {

  if (!key)
    return false;
  bool trigger_press = false;
  if (pending_hotkey_vk != 0 && key->text_len == 1) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (now - pending_hotkey_timeMs < 800) {
      unsigned char c = (unsigned char)key->text[0];
      int vkc = 0;
      if (c >= 'a' && c <= 'z')
        vkc = c - 'a' + 'A';
      else if (c >= 'A' && c <= 'Z')
        vkc = c;
      else if (c == ' ')
        vkc = VK_SPACE;
      else if (c == '[' || c == '{')
        vkc = VK_OEM_4;
      else if (c == ']' || c == '}')
        vkc = VK_OEM_6;
      else if (c == ';' || c == ':')
        vkc = VK_OEM_1;
      else if (c == '/' || c == '?')
        vkc = VK_OEM_2;
      if (vkc != 0 && vkc == pending_hotkey_vk)
        trigger_press = true;
    }
    pending_hotkey_vk = 0;
    pending_hotkey_mod = 0;
  }
  bool swallow = !trigger_press && (key->ctrl || key->alt);

  return swallow;
}
