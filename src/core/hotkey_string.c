#include "helpers/helpers_text.h"
#include "core/hotkey_manager.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "core/types/keys.h"
#include "platform/platform.h"
#include "helpers/helpers_input_state.h"

static int hotkey_mouse_token_to_button(const char *token) {
  if (!token || !token[0])
    return 0;

  static const struct {
    const char *name;
    int btn;
  } named[] = {
      {"MouseLeft", MOUSE_BTN_LEFT},
      {"LeftMouseButton", MOUSE_BTN_LEFT},
      {"MouseRight", MOUSE_BTN_RIGHT},
      {"RightMouseButton", MOUSE_BTN_RIGHT},
      {"MouseMiddle", MOUSE_BTN_MIDDLE},
      {"MiddleMouseButton", MOUSE_BTN_MIDDLE},
      {"MouseSide1", MOUSE_BTN_SIDE1},
      {"MouseSide2", MOUSE_BTN_SIDE2},
      {"MouseBack", MOUSE_BTN_BACK},
      {"MouseForward", MOUSE_BTN_FORWARD},
      {"MouseTask", MOUSE_BTN_TASK},
  };
  for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
    if (strcasecmp(token, named[i].name) == 0)
      return named[i].btn;
  }

  if (string_contains_ci(token, "Mouse") ||
      string_contains_ci(token, "MouseButton")) {
    int n = 0;
    for (const char *p = token; *p; p++) {
      if (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        if (n > MOUSE_BTN_MAX)
          return 0;
      }
    }
    if (n >= 1 && n <= MOUSE_BTN_MAX)
      return n;
  }
  return 0;
}

static int hotkey_named_key_to_vk(const char *token) {
  if (!token || !token[0])
    return 0;

  static const struct {
    const char *name;
    int vk;
  } named[] = {
      {"Space", VK_SPACE},
      {"Tab", VK_TAB},
      {"Enter", VK_RETURN},
      {"Return", VK_RETURN},
      {"Escape", VK_ESCAPE},
      {"Esc", VK_ESCAPE},
      {"Backspace", VK_BACK},
      {"Back", VK_BACK},
      {"Delete", VK_DELETE},
      {"Del", VK_DELETE},
      {"Home", VK_HOME},
      {"End", VK_END},
      {"PageUp", VK_PRIOR},
      {"Prior", VK_PRIOR},
      {"PageDown", VK_NEXT},
      {"Next", VK_NEXT},
      {"Up", VK_UP},
      {"ArrowUp", VK_UP},
      {"Down", VK_DOWN},
      {"ArrowDown", VK_DOWN},
      {"Left", VK_LEFT},
      {"ArrowLeft", VK_LEFT},
      {"Right", VK_RIGHT},
      {"ArrowRight", VK_RIGHT},
      {"LeftBracket", VK_OEM_4},
      {"RightBracket", VK_OEM_6},
      {"Semicolon", VK_OEM_1},
      {"Slash", VK_OEM_2},
      {"Grave", VK_OEM_3},
      {"Tilde", VK_OEM_3},
      {"Backslash", VK_OEM_5},
      {"Pipe", VK_OEM_5},
      {"Quote", VK_OEM_7},
      {"Comma", VK_OEM_COMMA},
      {"Period", VK_OEM_PERIOD},
      {"Minus", VK_OEM_MINUS},
      {"Plus", VK_OEM_PLUS},
  };
  for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
    if (strcasecmp(token, named[i].name) == 0)
      return named[i].vk;
  }

  if (token[1] == '\0') {
    switch (token[0]) {
    case '[':
    case '{':
      return VK_OEM_4;
    case ']':
    case '}':
      return VK_OEM_6;
    case ';':
      return VK_OEM_1;
    case '/':
      return VK_OEM_2;
    case '`':
      return VK_OEM_3;
    case '\\':
      return VK_OEM_5;
    case '\'':
    case '"':
      return VK_OEM_7;
    case ',':
      return VK_OEM_COMMA;
    case '.':
      return VK_OEM_PERIOD;
    case '-':
      return VK_OEM_MINUS;
    case '=':
      return VK_OEM_PLUS;
    default:
      break;
    }
  }

  if (string_contains_ci(token, "OEM_4") || string_contains_ci(token, "Oem4"))
    return VK_OEM_4;
  if (string_contains_ci(token, "OEM_5") || string_contains_ci(token, "Oem5"))
    return VK_OEM_5;
  if (string_contains_ci(token, "OEM_6") || string_contains_ci(token, "Oem6"))
    return VK_OEM_6;
  if (string_contains_ci(token, "OEM_1") || string_contains_ci(token, "Oem1"))
    return VK_OEM_1;
  if (string_contains_ci(token, "OEM_2") || string_contains_ci(token, "Oem2"))
    return VK_OEM_2;
  if (string_contains_ci(token, "OEM_3") || string_contains_ci(token, "Oem3"))
    return VK_OEM_3;
  if (string_contains_ci(token, "OEM_7") || string_contains_ci(token, "Oem7"))
    return VK_OEM_7;
  if (string_contains_ci(token, "OEM_COMMA") ||
      string_contains_ci(token, "OemComma"))
    return VK_OEM_COMMA;
  if (string_contains_ci(token, "OEM_PERIOD") ||
      string_contains_ci(token, "OemPeriod"))
    return VK_OEM_PERIOD;
  if (string_contains_ci(token, "OEM_MINUS") ||
      string_contains_ci(token, "OemMinus"))
    return VK_OEM_MINUS;
  if (string_contains_ci(token, "OEM_PLUS") ||
      string_contains_ci(token, "OemPlus"))
    return VK_OEM_PLUS;

  if (strcasecmp(token, "F1") == 0)
    return VK_F1;
  if (toupper((unsigned char)token[0]) == 'F' &&
      isdigit((unsigned char)token[1])) {
    int n = atoi(token + 1);
    if (n >= 1 && n <= 12)
      return VK_F1 + (n - 1);
  }
  return 0;
}

static int hotkey_token_to_vk(const char *token, int *out_mods) {
  if (!token || !token[0])
    return 0;

  if (strcasecmp(token, "RShift") == 0 || strcasecmp(token, "RightShift") == 0)
    return VK_RSHIFT;
  if (strcasecmp(token, "LShift") == 0 || strcasecmp(token, "LeftShift") == 0)
    return VK_LSHIFT;
  if (strcasecmp(token, "RCtrl") == 0 || strcasecmp(token, "RightCtrl") == 0 ||
      strcasecmp(token, "RControl") == 0 ||
      strcasecmp(token, "RightControl") == 0)
    return VK_RCONTROL;
  if (strcasecmp(token, "LCtrl") == 0 || strcasecmp(token, "LeftCtrl") == 0 ||
      strcasecmp(token, "LControl") == 0 ||
      strcasecmp(token, "LeftControl") == 0)
    return VK_LCONTROL;
  if (strcasecmp(token, "RAlt") == 0 || strcasecmp(token, "RightAlt") == 0 ||
      strcasecmp(token, "RMenu") == 0)
    return VK_RMENU;
  if (strcasecmp(token, "LAlt") == 0 || strcasecmp(token, "LeftAlt") == 0 ||
      strcasecmp(token, "LMenu") == 0)
    return VK_LMENU;
  if (strcasecmp(token, "RWin") == 0 || strcasecmp(token, "RightWin") == 0 ||
      strcasecmp(token, "RSuper") == 0 ||
      strcasecmp(token, "RightSuper") == 0 || strcasecmp(token, "RMeta") == 0)
    return VK_RWIN;
  if (strcasecmp(token, "LWin") == 0 || strcasecmp(token, "LeftWin") == 0 ||
      strcasecmp(token, "LSuper") == 0 || strcasecmp(token, "LeftSuper") == 0 ||
      strcasecmp(token, "LMeta") == 0)
    return VK_LWIN;

  if (string_contains_ci(token, "Ctrl") ||
      string_contains_ci(token, "Control")) {
    if (out_mods)
      *out_mods |= HOTKEY_MOD_CTRL;
    return 0;
  }
  if (string_contains_ci(token, "Shift")) {
    if (out_mods)
      *out_mods |= HOTKEY_MOD_SHIFT;
    return 0;
  }
  if (string_contains_ci(token, "Alt")) {
    if (out_mods)
      *out_mods |= HOTKEY_MOD_ALT;
    return 0;
  }
  if (string_contains_ci(token, "Win") || string_contains_ci(token, "Super") ||
      string_contains_ci(token, "Meta")) {
    if (out_mods)
      *out_mods |= HOTKEY_MOD_SUPER;
    return 0;
  }

  int mouse_btn = hotkey_mouse_token_to_button(token);
  if (mouse_btn > 0)
    return MOUSE_VK(mouse_btn);

  int named_vk = hotkey_named_key_to_vk(token);
  if (named_vk != 0)
    return named_vk;
  return toupper((unsigned char)token[0]);
}

bool hotkey_string_parse(const char *str, HotkeyKey *out) {
  if (out) {
    out->vk_code = 0;
    out->mods = 0;
  }
  if (!str || !str[0])
    return false;

  char buffer[256];
  safe_copy(buffer, sizeof(buffer), str);

  size_t len = strlen(buffer);
  bool ends_with_plus = false;
  if (len > 2 && buffer[len - 1] == '+' && buffer[len - 2] == '+') {
    ends_with_plus = true;
    buffer[len - 2] = '\0';
  }

  int out_key = 0;
  int mods = 0;

  char *save = NULL;
  const char *token = strtok_r(buffer, "+", &save);
  while (token != NULL) {
    int vk = hotkey_token_to_vk(token, &mods);
    if (vk != 0)
      out_key = vk;
    token = strtok_r(NULL, "+", &save);
  }

  if (ends_with_plus)
    out_key = VK_OEM_PLUS;

  if (out) {
    out->vk_code = out_key;
    out->mods = mods;
  }
  return out_key != 0;
}

static int hotkey_vk_to_name(int vk, char *out, int out_size) {
  if (out_size <= 0)
    return 0;

  const char *name = NULL;
  char tmp[16];

  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
    tmp[0] = (char)vk;
    tmp[1] = '\0';
    name = tmp;
  } else {
    switch (vk) {
    case VK_SPACE:
      name = "Space";
      break;
    case VK_TAB:
      name = "Tab";
      break;
    case VK_RETURN:
      name = "Enter";
      break;
    case VK_ESCAPE:
      name = "Escape";
      break;
    case VK_BACK:
      name = "Backspace";
      break;
    case VK_DELETE:
      name = "Delete";
      break;
    case VK_HOME:
      name = "Home";
      break;
    case VK_END:
      name = "End";
      break;
    case VK_PRIOR:
      name = "PageUp";
      break;
    case VK_NEXT:
      name = "PageDown";
      break;
    case VK_UP:
      name = "Up";
      break;
    case VK_DOWN:
      name = "Down";
      break;
    case VK_LEFT:
      name = "Left";
      break;
    case VK_RIGHT:
      name = "Right";
      break;
    case VK_LCONTROL:
      name = "LCtrl";
      break;
    case VK_RCONTROL:
      name = "RCtrl";
      break;
    case VK_LSHIFT:
      name = "LShift";
      break;
    case VK_RSHIFT:
      name = "RShift";
      break;
    case VK_LMENU:
      name = "LAlt";
      break;
    case VK_RMENU:
      name = "RAlt";
      break;
    case VK_LWIN:
      name = "LWin";
      break;
    case VK_RWIN:
      name = "RWin";
      break;
    case VK_OEM_1:
      name = ";";
      break;
    case VK_OEM_2:
      name = "/";
      break;
    case VK_OEM_3:
      name = "`";
      break;
    case VK_OEM_4:
      name = "[";
      break;
    case VK_OEM_5:
      name = "\\";
      break;
    case VK_OEM_6:
      name = "]";
      break;
    case VK_OEM_7:
      name = "'";
      break;
    case VK_OEM_COMMA:
      name = ",";
      break;
    case VK_OEM_PERIOD:
      name = ".";
      break;
    case VK_OEM_MINUS:
      name = "-";
      break;
    case VK_OEM_PLUS:
      name = "=";
      break;
    default:
      break;
    }
  }

  if (!name && vk >= VK_F1 && vk <= VK_F1 + 11) {
    snprintf(tmp, sizeof(tmp), "F%d", vk - VK_F1 + 1);
    name = tmp;
  }

  if (!name && vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + MOUSE_BTN_MAX - 1) {
    snprintf(tmp, sizeof(tmp), "Mouse%d", vk - VK_MOUSE_BASE + 1);
    name = tmp;
  }

  if (!name) {
    snprintf(tmp, sizeof(tmp), "%d", vk);
    name = tmp;
  }

  int written = snprintf(out, (size_t)out_size, "%s", name);
  if (written < 0 || written >= out_size)
    return 0;
  return written;
}

bool hotkey_string_format(HotkeyKey key, char *out, int out_size) {
  if (!out || out_size <= 0)
    return false;
  out[0] = '\0';
  if (key.vk_code == 0)
    return false;

  char key_name[32];
  if (hotkey_vk_to_name(key.vk_code, key_name, (int)sizeof(key_name)) == 0) {
    return false;
  }

  char mods_part[64];
  mods_part[0] = '\0';
  if (key.mods & HOTKEY_MOD_CTRL)
    strcat(mods_part, "Ctrl+");
  if (key.mods & HOTKEY_MOD_SHIFT)
    strcat(mods_part, "Shift+");
  if (key.mods & HOTKEY_MOD_ALT)
    strcat(mods_part, "Alt+");
  if (key.mods & HOTKEY_MOD_SUPER)
    strcat(mods_part, "Super+");

  int written = snprintf(out, (size_t)out_size, "%s%s", mods_part, key_name);
  if (written < 0 || written >= out_size) {
    out[0] = '\0';
    return false;
  }
  return true;
}
