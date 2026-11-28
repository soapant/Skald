#include "helpers/helpers_text.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_input_state.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "core/owner_registry.h"
#include "core/widget_registry.h"
#include "platform/platform.h"
#include "core/types/keys.h"
#include "deps/log.h"
#include "helpers/helpers_owner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deps/stb_ds.h"
#include "core/owner_purge.h"

#define SNIPPET_BUF_CAP 128

static SnippetBinding *snippets;
#define snippet_count arrlenu(snippets)
static SnippetFireFn fire_fn = NULL;

static char buf[SNIPPET_BUF_CAP];
static int buf_len = 0;

void snippet_set_fire_fn(SnippetFireFn fn) { fire_fn = fn; }

void snippet_fire(int idx, int del) {
  const SnippetBinding *b = snippet_get_binding(idx);
  if (!b || !b->trigger) {
    return;
  }
  for (int i = 0; i < del; i++) {
    Platform_EmitKey(VK_BACK, 1);
    Platform_EmitKey(VK_BACK, 0);
  }
  if (b->text)
    type_text_common(b->text);
  else if (b->func)
    b->func();
}

void snippet_reset_buffer(void) { buf_len = 0; }

const SnippetBinding *snippet_get_binding(int index) {
  if (index < 0 || (size_t)index >= snippet_count)
    return NULL;
  return &snippets[index];
}

int snippet_binding_count(void) { return snippet_count; }

static void snippet_free_binding(SnippetBinding *b) {
  if (!b)
    return;
  free(b->trigger);
  free(b->text);
  for (int j = 0; j < b->window_count; j++)
    free(b->windows[j]);
  free(b->windows);
  b->trigger = NULL;
  b->text = NULL;
  b->windows = NULL;
  b->window_count = 0;
  owner_tag_clear(&b->tag);
}

static void snippet_owner_set(OwnerTag *tag, void (*func)(void), void *caller) {
  char *owner =
      func ? owner_resolve_for_func_alloc((void *)(uintptr_t)func) : NULL;
  if ((!owner || !owner[0]) && caller)
    owner = owner_resolve_for_func_alloc(caller);
  owner_tag_set(tag, owner, 0);
  owner_strfree(&owner);
}

bool snippet_register_internal(SnippetRegisterInternal *argspec) {
  if (!argspec->trigger || !argspec->trigger[0])
    return false;
  if (!argspec->text && !argspec->func)
    return false;

  for (size_t i = 0; i < snippet_count; i++) {
    if (snippets[i].trigger && strcmp(snippets[i].trigger, argspec->trigger) == 0) {
      int nwin = 0;
      if (argspec->windows) {
        while (argspec->windows[nwin])
          nwin++;
      }
      char *nt = strdup(argspec->trigger);
      if (!nt)
        return false;
      char *ntext = argspec->text ? strdup(argspec->text) : NULL;
      if (argspec->text && !ntext) {
        free(nt);
        return false;
      }
      char **nwindows = NULL;
      if (nwin > 0) {
        nwindows = (char **)calloc((size_t)nwin, sizeof(char *));
        if (!nwindows) {
          free(nt);
          free(ntext);
          return false;
        }
        for (int j = 0; j < nwin; j++) {
          nwindows[j] = strdup(argspec->windows[j] ? argspec->windows[j] : "");
          if (!nwindows[j]) {
            for (int k = 0; k < j; k++)
              free(nwindows[k]);
            free(nwindows);
            free(nt);
            free(ntext);
            return false;
          }
        }
      }
      snippet_free_binding(&snippets[i]);
      snippets[i].trigger = nt;
      snippets[i].text = ntext;
      snippets[i].func = argspec->func;
      snippets[i].whitelist = argspec->whitelist;
      snippets[i].windows = nwindows;
      snippets[i].window_count = nwindows ? nwin : 0;
      snippet_owner_set(&snippets[i].tag, argspec->func, argspec->caller);
      snippet_reset_buffer();
      return true;
    }
  }
  SnippetBinding nb;
  SnippetBinding *b = &nb;
  memset(b, 0, sizeof(*b));
  b->trigger = strdup(argspec->trigger);
  if (!b->trigger)
    return false;
  b->text = argspec->text ? strdup(argspec->text) : NULL;
  if (argspec->text && !b->text) {
    free(b->trigger);
    return false;
  }
  b->func = argspec->func;
  b->whitelist = argspec->whitelist;
  b->windows = NULL;
  b->window_count = 0;
  if (argspec->windows) {
    int n = 0;
    while (argspec->windows[n])
      n++;
    if (n > 0) {
      b->windows = malloc((size_t)n * sizeof(char *));
      if (!b->windows) {
        free(b->trigger);
        free(b->text);
        return false;
      }
      for (int j = 0; j < n; j++) {
        b->windows[j] = strdup(argspec->windows[j] ? argspec->windows[j] : "");
        if (!b->windows[j]) {
          for (int k = 0; k < j; k++)
            free(b->windows[k]);
          free(b->windows);
          free(b->trigger);
          free(b->text);
          return false;
        }
      }
      b->window_count = n;
    }
  }
  snippet_owner_set(&b->tag, argspec->func, argspec->caller);
  arrput(snippets, nb);
  snippet_reset_buffer();
  return true;
}

bool snippet_unregister_internal(const char *trigger) {
  if (!trigger || !trigger[0])
    return false;
  for (size_t i = 0; i < snippet_count;) {
    if (snippets[i].trigger && strcmp(snippets[i].trigger, trigger) == 0) {
      snippet_free_binding(&snippets[i]);
      arrdel(snippets, i);
      return true;
    }
    i++;
  }
  return false;
}

static void snippet_destroy_cb(void *element) {
  SnippetBinding *b = (SnippetBinding *)element;
  log_info("snippet '%s' purged (owner unloaded)",
           b->trigger ? b->trigger : "?");
  snippet_free_binding(b);
}

OWNER_PURGE_REGISTER2(snippet_purge_owner)

void snippet_purge_owner(const char *path_a, const char *path_b) {
  size_t dropped = OWNER_PURGE_STRUCT(snippets, SnippetBinding, path_a, path_b,
                                      0, snippet_destroy_cb);
  if (dropped > 0) {
    log_info("snippet purged %zu binding%s", dropped, dropped == 1 ? "" : "s");
  }
}

static char snippet_char_for_vk(int vk, bool shift) {
  if (vk >= 'A' && vk <= 'Z')
    return shift ? (char)vk : (char)(vk - 'A' + 'a');
  if (vk >= '0' && vk <= '9') {
    if (!shift)
      return (char)vk;
    switch (vk) {
    case '1':
      return '!';
    case '2':
      return '@';
    case '3':
      return '#';
    case '4':
      return '$';
    case '5':
      return '%';
    case '6':
      return '^';
    case '7':
      return '&';
    case '8':
      return '*';
    case '9':
      return '(';
    case '0':
      return ')';
    }
    return 0;
  }
  if (vk == VK_SPACE)
    return ' ';
  switch (vk) {
  case VK_OEM_MINUS:
    return shift ? '_' : '-';
  case VK_OEM_PLUS:
    return shift ? '+' : '=';
  case VK_OEM_4:
    return shift ? '{' : '[';
  case VK_OEM_6:
    return shift ? '}' : ']';
  case VK_OEM_1:
    return shift ? ':' : ';';
  case VK_OEM_7:
    return shift ? '"' : '\'';
  case VK_OEM_COMMA:
    return shift ? '<' : ',';
  case VK_OEM_PERIOD:
    return shift ? '>' : '.';
  case VK_OEM_2:
    return shift ? '?' : '/';
  case VK_OEM_3:
    return shift ? '~' : '`';
  }
  return 0;
}

static void snippet_push_char(char c) {
  if (buf_len >= SNIPPET_BUF_CAP) {
    memmove(buf, buf + 1, SNIPPET_BUF_CAP - 1);
    buf_len = SNIPPET_BUF_CAP - 1;
  }
  buf[buf_len++] = c;
}

static int snippet_match(void) {
  for (size_t i = 0; i < snippet_count; i++) {
    if (!snippets[i].trigger || !snippets[i].trigger[0])
      continue;
    size_t tlen = strlen(snippets[i].trigger);
    if ((int)tlen > buf_len)
      continue;
    if (memcmp(buf + buf_len - (int)tlen, snippets[i].trigger, tlen) == 0)
      return i;
  }
  return -1;
}

bool snippet_feed_vk(int vk, int value, bool can_swallow) {
  if (value == 0 || input_key_is_window_escape(vk) ||
      widget_key_capture_active())
    return false;
  if (vk == VK_BACK) {
    if (buf_len > 0)
      buf_len--;
    return false;
  }
  if (vk == VK_RETURN || vk == VK_ESCAPE) {
    snippet_reset_buffer();
    return false;
  }
  int mods = remap_current_mods();
  if (mods & (HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT | HOTKEY_MOD_SUPER))
    return false;
  char c = snippet_char_for_vk(vk, (mods & HOTKEY_MOD_SHIFT) != 0);
  if (!c)
    return false;
  snippet_push_char(c);
  int idx = snippet_match();
  if (idx < 0)
    return false;
  const char *title = NULL;
  if (snippets[idx].window_count > 0)
    title = Platform_GetActiveWindowTitle();
  bool allowed =
      window_list_allows(snippets[idx].window_count, snippets[idx].whitelist,
                         snippets[idx].windows, title);
  if (!allowed)
    return false;
  size_t tlen = strlen(snippets[idx].trigger);
  int del = (int)tlen - (can_swallow ? 1 : 0);
  if (del < 0)
    del = 0;
  snippet_reset_buffer();
  if (fire_fn)
    fire_fn(idx, del);
  return true;
}

