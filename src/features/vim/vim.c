#include "core/flapi.h"

typedef enum { VIM_MODE_INSERT, VIM_MODE_NORMAL, VIM_MODE_ANY } VimMode;
static void vim_enter_normal_mode(void);

typedef void (*VimMotionCallback)(void);

#define VIM_MAX_MOTIONS 320
#define VIM_MAX_TOKENS 8
#define VIM_BUFFER_TOKENS 16

typedef struct {
  char tokens[VIM_MAX_TOKENS][32];
  int token_count;
  VimMode mode;
  VimMotionCallback cb;
} VimMotion;

static VimMode g_vim_mode = VIM_MODE_INSERT;
static VimMotion g_motions[VIM_MAX_MOTIONS];
static int g_motion_count = 0;
static char g_key_buffer[VIM_BUFFER_TOKENS][32];
static int g_key_buffer_count = 0;
static volatile int g_pending_active = 0;
static char g_pending_char = 0;
static FlMutex g_pending_mutex = NULL;

static void vim_reset_buffer(void) {
  g_key_buffer_count = 0;
}

static void vim_refresh_prompt(void) {
  PromptState *aps = fl_get_active_prompt();
  if (aps && aps->callbacks && aps->callbacks->refresh)
    aps->callbacks->refresh(aps);
}

static void vim_insert_char_silent(char ch) {
  PromptState *ps = fl_get_active_prompt();
  if (!ps || !ps->editbox)
    return;
  size_t len = strlen(ps->editbox->filter);
  if (len + 1 >= sizeof(ps->editbox->filter))
    return;
  if (ps->editbox->cursor > len)
    ps->editbox->cursor = len;
  memmove(ps->editbox->filter + ps->editbox->cursor + 1,
          ps->editbox->filter + ps->editbox->cursor,
          len - ps->editbox->cursor + 1);
  ps->editbox->filter[ps->editbox->cursor] = ch;
  ps->editbox->cursor++;
  fl_invalidate_prompt();
}

static void *vim_pending_thread(void *arg) {
  fl_sleep(0.3);
  fl_mutex_lock(g_pending_mutex);
  char pc = g_pending_char;
  if (g_pending_active) {
    g_pending_active = 0;
    g_pending_char = 0;
    fl_mutex_unlock(g_pending_mutex);
    vim_refresh_prompt();
  } else {
    fl_mutex_unlock(g_pending_mutex);
  }
  return NULL;
}

static void vim_cancel_pending(void) {
  fl_mutex_lock(g_pending_mutex);
  g_pending_active = 0;
  fl_mutex_unlock(g_pending_mutex);
}

static void vim_start_pending(char ch) {
  fl_mutex_lock(g_pending_mutex);
  g_pending_active = 1;
  g_pending_char = ch;
  fl_mutex_unlock(g_pending_mutex);
  fl_thread_create(vim_pending_thread, NULL);
}

static void vim_delete_last(int n) {
  PromptState *ps = fl_get_active_prompt();
  if (!ps || !ps->editbox)
    return;
  int len = (int)strlen(ps->editbox->filter);
  if (n > len)
    n = len;
  if (n <= 0)
    return;
  ps->editbox->filter[len - n] = '\0';
  if (ps->editbox->cursor > len - n)
    ps->editbox->cursor = len - n;
  fl_invalidate_prompt();
}

static void vim_motion(const char *seq, VimMode mode, VimMotionCallback cb) {
  if (!seq || !cb || g_motion_count >= VIM_MAX_MOTIONS)
    return;
  VimMotion *m = &g_motions[g_motion_count++];
  m->mode = mode;
  m->cb = cb;
  m->token_count = 0;
  if (strchr(seq, ' ')) {
    char tmp[256];
    strncpy(tmp, seq, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, " ", &save);
    while (tok && m->token_count < VIM_MAX_TOKENS) {
      strncpy(m->tokens[m->token_count], tok, 31);
      m->tokens[m->token_count][31] = '\0';
      m->token_count++;
      tok = strtok_r(NULL, " ", &save);
    }
  } else if (strchr(seq, '+')) {
    strncpy(m->tokens[0], seq, 31);
    m->tokens[0][31] = '\0';
    m->token_count = 1;
  } else {
    for (const char *p = seq; *p && m->token_count < VIM_MAX_TOKENS; p++) {
      m->tokens[m->token_count][0] = *p;
      m->tokens[m->token_count][1] = '\0';
      m->token_count++;
    }
  }
}

static bool vim_key_to_token(const PopupKeyEvent *key, char *out, size_t outsz) {
  if (!key || !out || outsz == 0)
    return false;
  char ch = 0;
  if (key->vk >= 'A' && key->vk <= 'Z')
    ch = (char)(key->shift ? key->vk : key->vk + 32);
  else if (key->vk >= 'a' && key->vk <= 'z')
    ch = (char)key->vk;
  else
    return false;
  size_t pos = 0;
  if (key->ctrl) {
    memcpy(out + pos, "ctrl+", 5);
    pos += 5;
  }
  if (key->shift && !(key->vk >= 'A' && key->vk <= 'Z')) {
    memcpy(out + pos, "shift+", 6);
    pos += 6;
  }
  if (pos + 1 >= outsz)
    return false;
  out[pos++] = ch;
  out[pos] = '\0';
  return true;
}

static bool vim_key_interceptor(const PopupKeyEvent *key) {
  if (!key)
    return false;
  if (key->vk == VK_ESCAPE) {
    if (g_vim_mode == VIM_MODE_INSERT) {
      g_vim_mode = VIM_MODE_NORMAL;
      fl_get_metrics()->edit_cursor_width = 12;
      fl_invalidate_prompt();
      vim_reset_buffer();
      return true;
    }
    return false;
  }
  if (g_vim_mode == VIM_MODE_INSERT) {
    char token[32];
    int has_token = vim_key_to_token(key, token, sizeof(token));

    fl_mutex_lock(g_pending_mutex);
    int was_pending = g_pending_active;
    char pending_char = g_pending_char;
    fl_mutex_unlock(g_pending_mutex);

    if (was_pending) {
      int continues = 0;
      for (int i = 0; i < g_motion_count; i++) {
        VimMotion *m = &g_motions[i];
        if (!(m->mode == g_vim_mode || m->mode == VIM_MODE_ANY))
          continue;
        if (m->token_count < 2)
          continue;
        if (m->tokens[0][0] == pending_char && m->tokens[0][1] == '\0' &&
            strcmp(m->tokens[1], token) == 0) {
          continues = 1;
          break;
        }
      }
      if (continues) {
        vim_cancel_pending();
        vim_delete_last(1);
        for (int i = 0; i < g_motion_count; i++) {
          VimMotion *m = &g_motions[i];
          if (!(m->mode == g_vim_mode || m->mode == VIM_MODE_ANY))
            continue;
          if (m->token_count < 2)
            continue;
          if (m->tokens[0][0] != pending_char || m->tokens[0][1] != '\0')
            continue;
          if (strcmp(m->tokens[1], token) != 0)
            continue;
          if (m->cb)
            m->cb();
          vim_refresh_prompt();
          return true;
        }
      }
      vim_cancel_pending();
    }

    if (has_token) {
      for (int i = 0; i < g_motion_count; i++) {
        VimMotion *m = &g_motions[i];
        if (!(m->mode == g_vim_mode || m->mode == VIM_MODE_ANY))
          continue;
        if (m->token_count < 2)
          continue;
        if (strcmp(m->tokens[0], token) == 0) {
          vim_insert_char_silent(token[0]);
          vim_start_pending(token[0]);
          return true;
        }
      }
    }
  }

  char token[32];
  if (vim_key_to_token(key, token, sizeof(token))) {
    if (g_key_buffer_count >= VIM_BUFFER_TOKENS)
      g_key_buffer_count = 0;
    strncpy(g_key_buffer[g_key_buffer_count], token, 31);
    g_key_buffer[g_key_buffer_count][31] = '\0';
    g_key_buffer_count++;

    for (int i = 0; i < g_motion_count; i++) {
      VimMotion *m = &g_motions[i];
      if (!(m->mode == g_vim_mode || m->mode == VIM_MODE_ANY))
        continue;
      if (m->token_count == 0 || m->token_count > g_key_buffer_count)
        continue;
      bool match = true;
      for (int t = 0; t < m->token_count; t++) {
        if (strcmp(g_key_buffer[g_key_buffer_count - m->token_count + t],
                    m->tokens[t]) != 0) {
          match = false;
          break;
        }
      }
      if (match) {
      if (g_vim_mode == VIM_MODE_INSERT)
        vim_delete_last(m->token_count);
      vim_reset_buffer();
      if (m->cb)
        m->cb();
      PromptState *aps = fl_get_active_prompt();
      if (aps && aps->callbacks && aps->callbacks->refresh)
        aps->callbacks->refresh(aps);
      return true;
    }
  }
  } else {
    vim_reset_buffer();
  }

  if (g_vim_mode == VIM_MODE_NORMAL) {
    if (key->vk == 'I' || key->vk == 'i') {
      g_vim_mode = VIM_MODE_INSERT;
      fl_get_metrics()->edit_cursor_width = 0;
      fl_invalidate_prompt();
      vim_reset_buffer();
      return true;
    }
    return false;
  }

  return false;
}

static void vim_enter_normal_mode(void) {
  g_vim_mode = VIM_MODE_NORMAL;
  fl_get_metrics()->edit_cursor_width = 12;
  fl_invalidate_prompt();
}

static void vim_on_prompt_open(void) {
  g_vim_mode = VIM_MODE_INSERT;
  fl_get_metrics()->edit_cursor_width = 0;
  fl_invalidate_prompt();
  vim_reset_buffer();
  vim_cancel_pending();
}

FLINT_FEATURE_INIT {
  g_pending_mutex = fl_mutex_create();
  fl_get_metrics()->edit_cursor_width = 0;
  vim_motion("jk", VIM_MODE_INSERT, vim_enter_normal_mode);
  fl_register_key_interceptor(vim_key_interceptor);
  fl_register_event(EVENT_PROMPT_OPEN, vim_on_prompt_open);
}
