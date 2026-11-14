#include "helpers/helpers_text.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "platform/platform.h"
#include "core/types/keys.h"
#include "deps/log.h"
#include "helpers/helpers_owner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNIPPET_MAX_BINDINGS 64
#define SNIPPET_BUF_CAP 128

static SnippetBinding snippets[SNIPPET_MAX_BINDINGS];
static int snippet_count = 0;
static SnippetFireFn fire_fn = NULL;

static char buf[SNIPPET_BUF_CAP];
static int buf_len = 0;

void snippet_set_fire_fn(SnippetFireFn fn) { fire_fn = fn; }

void snippet_reset_buffer(void) { buf_len = 0; }

const SnippetBinding *snippet_get_binding(int index) {
  if (index < 0 || index >= snippet_count)
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
}

bool snippet_register_internal(const char *trigger, const char *text,
                              void (*func)(void), bool whitelist,
                              const char *const *windows, void *caller) {
  if (!trigger || !trigger[0])
    return false;
  if (!text && !func)
    return false;

  char owner[OWNER_PATH_MAX];
  owner[0] = '\0';
  if (func)
    owner_resolve_for_func((void *)(uintptr_t)func, owner, sizeof(owner));
  if (!owner[0] && caller)
    owner_resolve_for_func(caller, owner, sizeof(owner));

  for (int i = 0; i < snippet_count; i++) {
    if (snippets[i].trigger && strcmp(snippets[i].trigger, trigger) == 0) {
      int nwin = 0;
      if (windows) {
        while (windows[nwin])
          nwin++;
      }
      char *nt = strdup(trigger);
      if (!nt)
        return false;
      char *ntext = text ? strdup(text) : NULL;
      if (text && !ntext) {
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
          nwindows[j] = strdup(windows[j] ? windows[j] : "");
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
      snippets[i].func = func;
      snippets[i].whitelist = whitelist;
      snippets[i].windows = nwindows;
      snippets[i].window_count = nwindows ? nwin : 0;
      snprintf(snippets[i].owner, sizeof(snippets[i].owner), "%s", owner);
      snippet_reset_buffer();
      return true;
    }
  }
  if (snippet_count >= SNIPPET_MAX_BINDINGS)
    return false;
  SnippetBinding *b = &snippets[snippet_count];
  memset(b, 0, sizeof(*b));
  b->trigger = strdup(trigger);
  if (!b->trigger)
    return false;
  b->text = text ? strdup(text) : NULL;
  if (text && !b->text) {
    free(b->trigger);
    return false;
  }
  b->func = func;
  b->whitelist = whitelist;
  b->windows = NULL;
  b->window_count = 0;
  snprintf(b->owner, sizeof(b->owner), "%s", owner);
  if (windows) {
    int n = 0;
    while (windows[n])
      n++;
    if (n > 0) {
      b->windows = malloc((size_t)n * sizeof(char *));
      if (!b->windows) {
        free(b->trigger);
        free(b->text);
        return false;
      }
      for (int j = 0; j < n; j++) {
        b->windows[j] = strdup(windows[j] ? windows[j] : "");
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
  snippet_count++;
  snippet_reset_buffer();
  return true;
}

bool snippet_unregister_internal(const char *trigger) {
  if (!trigger || !trigger[0])
    return false;
  int n = 0;
  bool found = false;
  for (int i = 0; i < snippet_count; i++) {
    if (snippets[i].trigger && strcmp(snippets[i].trigger, trigger) == 0) {
      snippet_free_binding(&snippets[i]);
      found = true;
      continue;
    }
    if (n != i)
      snippets[n] = snippets[i];
    n++;
  }
  if (n != snippet_count)
    memset(&snippets[n], 0,
           (size_t)(snippet_count - n) * sizeof(SnippetBinding));
  snippet_count = n;
  return found;
}

void snippet_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;

  int n = 0;
  for (int i = 0; i < snippet_count; i++) {
    if (owner_module_matches(snippets[i].owner, path_a, path_b)) {
      log_info("snippet '%s' purged (owner unloaded)",
               snippets[i].trigger ? snippets[i].trigger : "?");
      snippet_free_binding(&snippets[i]);
      continue;
    }
    if (n != i)
      snippets[n] = snippets[i];
    n++;
  }
  int dropped = snippet_count - n;
  if (dropped > 0) {
    memset(&snippets[n], 0,
           (size_t)(snippet_count - n) * sizeof(SnippetBinding));
    snippet_count = n;
    log_info("snippet purged %d binding%s", dropped, dropped == 1 ? "" : "s");
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
  for (int i = 0; i < snippet_count; i++) {
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

bool snippet_feed_vk(int vk_code, int value, bool can_swallow) {
  if (value == 0)
    return false;
  if (vk_code == VK_BACK) {
    if (buf_len > 0)
      buf_len--;
    return false;
  }
  if (vk_code == VK_RETURN || vk_code == VK_ESCAPE) {
    snippet_reset_buffer();
    return false;
  }
  int mods = remap_current_mods();
  if (mods & (HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT | HOTKEY_MOD_SUPER))
    return false;
  char c = snippet_char_for_vk(vk_code, (mods & HOTKEY_MOD_SHIFT) != 0);
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
