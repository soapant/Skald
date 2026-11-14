#include "core/remap_manager.h"

#include "core/types/keys.h"
#include "helpers/helpers_text.h"

#include <ctype.h>
#include <string.h>

static int remap_vk_from_token(const char *tok) {
  if (!tok || !tok[0])
    return 0;
  if (strlen(tok) == 1) {
    char c = tok[0];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      return toupper((unsigned char)c);
    if (c >= '0' && c <= '9')
      return c;
    if (c == ' ')
      return VK_SPACE;
  }
  if (strcasecmp(tok, "Space") == 0)
    return VK_SPACE;
  if (strcasecmp(tok, "Tab") == 0)
    return VK_TAB;
  if (strcasecmp(tok, "Enter") == 0 || strcasecmp(tok, "Return") == 0)
    return VK_RETURN;
  if (strcasecmp(tok, "Escape") == 0 || strcasecmp(tok, "Esc") == 0)
    return VK_ESCAPE;
  if (strcasecmp(tok, "Backspace") == 0 || strcasecmp(tok, "Back") == 0)
    return VK_BACK;
  if (strcasecmp(tok, "Delete") == 0 || strcasecmp(tok, "Del") == 0)
    return VK_DELETE;
  if (strcasecmp(tok, "Up") == 0 || strcasecmp(tok, "ArrowUp") == 0)
    return VK_UP;
  if (strcasecmp(tok, "Down") == 0 || strcasecmp(tok, "ArrowDown") == 0)
    return VK_DOWN;
  if (strcasecmp(tok, "Left") == 0 || strcasecmp(tok, "ArrowLeft") == 0)
    return VK_LEFT;
  if (strcasecmp(tok, "Right") == 0 || strcasecmp(tok, "ArrowRight") == 0)
    return VK_RIGHT;
  if (strcasecmp(tok, "Home") == 0)
    return VK_HOME;
  if (strcasecmp(tok, "End") == 0)
    return VK_END;
  if (strcasecmp(tok, "PageUp") == 0 || strcasecmp(tok, "Prior") == 0)
    return VK_PRIOR;
  if (strcasecmp(tok, "PageDown") == 0 || strcasecmp(tok, "Next") == 0)
    return VK_NEXT;
  if (strcasecmp(tok, "Grave") == 0 || strcasecmp(tok, "`") == 0 ||
      strcasecmp(tok, "Tilde") == 0 || strcasecmp(tok, "~") == 0)
    return VK_OEM_3;
  if (strcasecmp(tok, "Minus") == 0 || strcasecmp(tok, "-") == 0 ||
      strcasecmp(tok, "OEM_MINUS") == 0)
    return VK_OEM_MINUS;
  if (strcasecmp(tok, "Plus") == 0 || strcasecmp(tok, "=") == 0 ||
      strcasecmp(tok, "+") == 0 || strcasecmp(tok, "OEM_PLUS") == 0)
    return VK_OEM_PLUS;
  if (strcasecmp(tok, "Semicolon") == 0 || strcasecmp(tok, ";") == 0 ||
      strcasecmp(tok, ":") == 0 || strcasecmp(tok, "OEM_1") == 0)
    return VK_OEM_1;
  if (strcasecmp(tok, "Slash") == 0 || strcasecmp(tok, "/") == 0 ||
      strcasecmp(tok, "?") == 0 || strcasecmp(tok, "OEM_2") == 0)
    return VK_OEM_2;
  if (strcasecmp(tok, "LeftBracket") == 0 || strcasecmp(tok, "[") == 0 ||
      strcasecmp(tok, "{") == 0 || strcasecmp(tok, "OEM_4") == 0)
    return VK_OEM_4;
  if (strcasecmp(tok, "RightBracket") == 0 || strcasecmp(tok, "]") == 0 ||
      strcasecmp(tok, "}") == 0 || strcasecmp(tok, "OEM_6") == 0)
    return VK_OEM_6;
  if (strcasecmp(tok, "Quote") == 0 || strcasecmp(tok, "'") == 0 ||
      strcasecmp(tok, "\"") == 0 || strcasecmp(tok, "OEM_7") == 0)
    return VK_OEM_7;
  if (strcasecmp(tok, "Comma") == 0 || strcasecmp(tok, ",") == 0 ||
      strcasecmp(tok, "<") == 0 || strcasecmp(tok, "OEM_COMMA") == 0)
    return VK_OEM_COMMA;
  if (strcasecmp(tok, "Period") == 0 || strcasecmp(tok, ".") == 0 ||
      strcasecmp(tok, ">") == 0 || strcasecmp(tok, "OEM_PERIOD") == 0)
    return VK_OEM_PERIOD;
  if (strcasecmp(tok, "Backslash") == 0 || strcasecmp(tok, "\\") == 0 ||
      strcasecmp(tok, "Pipe") == 0 || strcasecmp(tok, "|") == 0 ||
      strcasecmp(tok, "OEM_5") == 0)
    return 0xDC;
  if (strcasecmp(tok, "F1") == 0)
    return VK_F1;
  if (strcasecmp(tok, "F2") == 0)
    return VK_F1 + 1;
  if (strcasecmp(tok, "F3") == 0)
    return VK_F1 + 2;
  if (strcasecmp(tok, "F4") == 0)
    return VK_F1 + 3;
  if (strcasecmp(tok, "F5") == 0)
    return VK_F1 + 4;
  if (strcasecmp(tok, "F6") == 0)
    return VK_F1 + 5;
  if (strcasecmp(tok, "F7") == 0)
    return VK_F1 + 6;
  if (strcasecmp(tok, "F8") == 0)
    return VK_F1 + 7;
  if (strcasecmp(tok, "F9") == 0)
    return VK_F1 + 8;
  if (strcasecmp(tok, "F10") == 0)
    return VK_F1 + 9;
  if (strcasecmp(tok, "F11") == 0)
    return VK_F1 + 10;
  if (strcasecmp(tok, "F12") == 0)
    return VK_F1 + 11;
  if (strlen(tok) == 1)
    return toupper((unsigned char)tok[0]);
  return 0;
}

bool remap_parse_triggers(const char *str, int *out_mods, int *out_vks,
                         int *out_count) {
  *out_mods = 0;
  *out_count = 0;
  if (!str)
    return false;
  char buf[256];
  safe_copy(buf, sizeof(buf), str);
  char *save = NULL;
  char *token = strtok_r(buf, "+", &save);
  while (token) {
    while (*token == ' ' || *token == '\t')
      token++;
    char *end = token + strlen(token) - 1;
    while (end > token && (*end == ' ' || *end == '\t')) {
      *end = '\0';
      end--;
    }
    if (strcasecmp(token, "Ctrl") == 0 || strcasecmp(token, "Control") == 0) {
      *out_mods |= HOTKEY_MOD_CTRL;
    } else if (strcasecmp(token, "Shift") == 0) {
      *out_mods |= HOTKEY_MOD_SHIFT;
    } else if (strcasecmp(token, "Alt") == 0) {
      *out_mods |= HOTKEY_MOD_ALT;
    } else if (strcasecmp(token, "Win") == 0 ||
               strcasecmp(token, "Super") == 0 ||
               strcasecmp(token, "Meta") == 0) {
      *out_mods |= HOTKEY_MOD_SUPER;
    } else if (strcasecmp(token, "LeftCtrl") == 0 ||
               strcasecmp(token, "LCtrl") == 0 ||
               strcasecmp(token, "LControl") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_LCONTROL;
    } else if (strcasecmp(token, "RightCtrl") == 0 ||
               strcasecmp(token, "RCtrl") == 0 ||
               strcasecmp(token, "RControl") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_RCONTROL;
    } else if (strcasecmp(token, "LeftShift") == 0 ||
               strcasecmp(token, "LShift") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_LSHIFT;
    } else if (strcasecmp(token, "RightShift") == 0 ||
               strcasecmp(token, "RShift") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_RSHIFT;
    } else if (strcasecmp(token, "LeftAlt") == 0 ||
               strcasecmp(token, "LAlt") == 0 ||
               strcasecmp(token, "LMenu") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_LMENU;
    } else if (strcasecmp(token, "RightAlt") == 0 ||
               strcasecmp(token, "RAlt") == 0 ||
               strcasecmp(token, "RMenu") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_RMENU;
    } else if (strcasecmp(token, "LeftSuper") == 0 ||
               strcasecmp(token, "LWin") == 0 ||
               strcasecmp(token, "LSuper") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_LWIN;
    } else if (strcasecmp(token, "RightSuper") == 0 ||
               strcasecmp(token, "RWin") == 0 ||
               strcasecmp(token, "RSuper") == 0) {
      if (*out_count < REMAP_PARSE_MAX_VKS)
        out_vks[(*out_count)++] = VK_RWIN;
    } else {
      int vk = 0;
      bool need_shift = false;
      if (strlen(token) == 1) {
        char c = token[0];
        switch (c) {
        case '!':
          vk = '1';
          need_shift = true;
          break;
        case '@':
          vk = '2';
          need_shift = true;
          break;
        case '#':
          vk = '3';
          need_shift = true;
          break;
        case '$':
          vk = '4';
          need_shift = true;
          break;
        case '%':
          vk = '5';
          need_shift = true;
          break;
        case '^':
          vk = '6';
          need_shift = true;
          break;
        case '&':
          vk = '7';
          need_shift = true;
          break;
        case '*':
          vk = '8';
          need_shift = true;
          break;
        case '(':
          vk = '9';
          need_shift = true;
          break;
        case ')':
          vk = '0';
          need_shift = true;
          break;
        case '_':
          vk = VK_OEM_MINUS;
          need_shift = true;
          break;
        case '+':
          vk = VK_OEM_PLUS;
          need_shift = true;
          break;
        case '~':
          vk = VK_OEM_3;
          need_shift = true;
          break;
        case '`':
          vk = VK_OEM_3;
          need_shift = false;
          break;
        case '{':
          vk = VK_OEM_4;
          need_shift = true;
          break;
        case '}':
          vk = VK_OEM_6;
          need_shift = true;
          break;
        case ':':
          vk = VK_OEM_1;
          need_shift = true;
          break;
        case '"':
          vk = VK_OEM_7;
          need_shift = true;
          break;
        case '<':
          vk = VK_OEM_COMMA;
          need_shift = true;
          break;
        case '>':
          vk = VK_OEM_PERIOD;
          need_shift = true;
          break;
        case '?':
          vk = VK_OEM_2;
          need_shift = true;
          break;
        case '|':
          vk = 0xDC;
          need_shift = true;
          break;
        case '\\':
          vk = 0xDC;
          need_shift = false;
          break;
        default:
          vk = 0;
          break;
        }
      }
      if (vk) {
        if (need_shift)
          *out_mods |= HOTKEY_MOD_SHIFT;
        if (*out_count < REMAP_PARSE_MAX_VKS)
          out_vks[(*out_count)++] = vk;
      } else {
        vk = remap_vk_from_token(token);
        if (vk) {
          if (*out_count < REMAP_PARSE_MAX_VKS)
            out_vks[(*out_count)++] = vk;
        }
      }
    }
    token = strtok_r(NULL, "+", &save);
  }
  return true;
}
