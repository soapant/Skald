#include "config_values.h"
#include "helpers/helpers_ui.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_input_state.h"
#include "core/context.h"
#include "core/types/keys.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "platform/lib.h"
#include "deps/log.h"

static atomic_flag lazy_mutex_flag = ATOMIC_FLAG_INIT;

void *lazy_mutex_get(void **slot) {
  if (!slot)
    return NULL;
  while (atomic_flag_test_and_set_explicit(&lazy_mutex_flag,
                                           memory_order_acquire)) {
  }
  if (!*slot)
    *slot = Platform_MutexCreate();
  void *mutex = *slot;
  atomic_flag_clear_explicit(&lazy_mutex_flag, memory_order_release);
  return mutex;
}

unsigned long rgb_to_hex(unsigned long win_color) {
#if defined(__linux__)
  unsigned long r = win_color & 0xFF;
  unsigned long g = (win_color >> 8) & 0xFF;
  unsigned long b = (win_color >> 16) & 0xFF;
  return (r << 16) | (g << 8) | b;
#elif defined(_WIN32)
  return win_color;
#endif
  return 0;
}

unsigned short opacity_alpha16(void) {
  float t = WindowOpacity / 255.0f;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return (unsigned short)(t * 65535.0f + 0.5f);
}

unsigned int color_lerp(unsigned int a, unsigned int b, float t) {
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF),
      ab = (int)(a & 0xFF);
  int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF),
      bb = (int)(b & 0xFF);
  unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
  unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
  unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
  return (rr << 16) | (rg << 8) | rb;
}

ColorRef color_lerp_bgr(ColorRef a, ColorRef b, float t) {
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  int ar = (int)(a & 0xFF), ag = (int)((a >> 8) & 0xFF),
      ab = (int)((a >> 16) & 0xFF);
  int br = (int)(b & 0xFF), bg = (int)((b >> 8) & 0xFF),
      bb = (int)((b >> 16) & 0xFF);
  unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
  unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
  unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
  return (ColorRef)(rr | (rg << 8) | (rb << 16));
}

ColorRef widget_accent(void) {
  return color_lerp(COLOR_BG, RGB(255, 255, 255), 0.66f);
}

ColorRef widget_neutral(void) {
  return color_lerp(COLOR_BG, RGB(255, 255, 255), 0.30f);
}

void widget_mouse_poll(WidgetMouseState *state, int *out_x, int *out_y,
                       bool *out_down) {
  if (!state->valid) {
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      int wx = 0, wy = 0;
      WindowManager *wm = WindowManager_Get();
      Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
      bool have_origin = false;
      if (top)
        have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
      if (!have_origin) {
        have_origin = Platform_WindowToRoot(
            SkaldContext_Get()->window.hPopupWnd, &wx, &wy);
      }
      if (have_origin) {
        mx -= wx;
        my -= wy;
      }
      int dx = 0, dy = 0;
      get_draw_offset(&dx, &dy);
      mx -= dx;
      my -= dy;
    } else {
      mx = -9999;
      my = -9999;
    }
    state->mx = mx;
    state->my = my;
    state->down = input_mouse_state_down(MOUSE_BTN_LEFT);
    state->valid = true;
  }
  if (out_x)
    *out_x = state->mx;
  if (out_y)
    *out_y = state->my;
  if (out_down)
    *out_down = state->down;
}

int monitor_count(void) {
  MonitorEntry mon[16];
  return monitor_fill(mon, 16);
}

bool monitor_geometry_at(int index, int *x, int *y, int *w, int *h) {
  MonitorEntry mon[16];
  int n = monitor_fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w)
    return false;
  if (x)
    *x = mon[index].x;
  if (y)
    *y = mon[index].y;
  if (w)
    *w = mon[index].w;
  if (h)
    *h = mon[index].h;
  return true;
}

bool monitor_info(int index, MonitorInfo *out) {
  MonitorEntry mon[16];
  int n = monitor_fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w || !out)
    return false;
  memset(out, 0, sizeof(*out));
  out->index = index;
  out->x = mon[index].x;
  out->y = mon[index].y;
  out->w = mon[index].w;
  out->h = mon[index].h;
  out->is_primary = mon[index].is_primary;
  snprintf(out->name, sizeof(out->name), "%s", mon[index].name);
  return true;
}

const char *compiler_find_usable(const char *const candidates[],
                                 bool (*probe)(const char *)) {
  for (size_t i = 0; candidates[i]; i++) {
    char *found = Platform_FindExecutable(candidates[i]);
    if (!found)
      continue;
    if (probe(found)) {
      log_info("feature builds will use compiler: %s", found);
      return found;
    }
    log_warn("skipping unusable compiler for feature builds: %s", found);
    free(found);
  }
  return NULL;
}

size_t compiler_argv_base(const char *cc, const char *argv[]) {
  size_t n = 0;
  argv[n++] = cc;
  argv[n++] = "-std=gnu17";
  argv[n++] = "-I";
  argv[n++] = "build/sdk";
  return n;
}

bool key_is_ctrl_pressed(void) {
  return input_is_key_down(VK_LCONTROL) || input_is_key_down(VK_RCONTROL) ||
         input_is_key_down(VK_CONTROL);
}

bool key_is_shift_pressed(void) {
  return input_is_key_down(VK_LSHIFT) || input_is_key_down(VK_RSHIFT) ||
         input_is_key_down(VK_SHIFT);
}

bool key_is_alt_pressed(void) {
  return input_is_key_down(VK_LMENU) || input_is_key_down(VK_RMENU) ||
         input_is_key_down(VK_MENU);
}

bool key_is_super_pressed(void) {
  return input_is_key_down(VK_LWIN) || input_is_key_down(VK_RWIN);
}

static KeyInterceptorFn key_interceptor = NULL;
static char *key_interceptor_owner = NULL;

void key_register_interceptor(KeyInterceptorFn fn) {
  key_interceptor = fn;
  owner_strfree(&key_interceptor_owner);
  if (fn)
    key_interceptor_owner = owner_resolve_for_func_alloc((void *)fn);
}

void key_purge_interceptor_owner(const char *path_a, const char *path_b) {
  if (key_interceptor &&
      owner_module_matches(key_interceptor_owner, path_a, path_b)) {
    key_interceptor = NULL;
    owner_strfree(&key_interceptor_owner);
  }
}

bool key_invoke_interceptor(const PopupKeyEvent *key) {
  if (!key_interceptor)
    return false;
  return key_interceptor(key);
}

char *owner_strdup(const char *src) {
  if (!src || !src[0])
    return NULL;
  size_t n = strlen(src);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, src, n + 1);
  return out;
}

void owner_strfree(char **src) {
  if (!src || !*src)
    return;
  free(*src);
  *src = NULL;
}

char *owner_module_basename_alloc(const char *p) {
  if (!p || !p[0])
    return NULL;
  const char *s = p;
  for (const char *q = p; *q; q++)
    if (*q == '/' || *q == '\\')
      s = q + 1;
  size_t n = strlen(s);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, s, n + 1);
  return out;
}

void owner_module_basename(const char *p, char *out, size_t n) {
  if (!out || n == 0)
    return;
  if (!p || !p[0]) {
    out[0] = '\0';
    return;
  }
  const char *s = p;
  for (const char *q = p; *q; q++)
    if (*q == '/' || *q == '\\')
      s = q + 1;
  snprintf(out, n, "%s", s);
}

bool owner_module_matches(const char *owner, const char *a, const char *b) {
  if (!owner || !owner[0])
    return false;
  if (a && a[0] && strcmp(owner, a) == 0)
    return true;
  if (b && b[0] && strcmp(owner, b) == 0)
    return true;
  char *ob = owner_module_basename_alloc(owner);
  if (!ob)
    return false;
  bool match = false;
  if (a && a[0]) {
    char *ab = owner_module_basename_alloc(a);
    if (ab) {
      match = strcmp(ob, ab) == 0;
      free(ab);
    }
  }
  if (!match && b && b[0]) {
    char *bb = owner_module_basename_alloc(b);
    if (bb) {
      match = strcmp(ob, bb) == 0;
      free(bb);
    }
  }
  free(ob);
  return match;
}

char *owner_resolve_for_func_alloc(void *fn) {
  if (!fn)
    return NULL;
  return Platform_GetModuleOwnerPathAlloc(fn);
}

void owner_resolve_for_func(void *fn, char *out, size_t n) {
  if (!out || n == 0)
    return;
  out[0] = '\0';
  if (!fn)
    return;
  Platform_GetModuleOwnerPath(fn, out, n);
  out[n - 1] = '\0';
}

bool is_safe_process_name(const char *name) {
  if (!name || !*name)
    return false;
  for (const char *p = name; *p; p++) {
    unsigned char c = (unsigned char)*p;
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!ok)
      return false;
  }
  return true;
}

bool lookup_ci_eq(const char *a, const char *b) {
  for (;;) {
    unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z')
      ca += 32;
    if (cb >= 'A' && cb <= 'Z')
      cb += 32;
    if (ca != cb)
      return false;
    if (ca == '\0')
      return true;
    a++;
    b++;
  }
}

char *lookup_first_line(char *out) {
  char *start = out;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    start++;
  char *end = start;
  while (*end && *end != '\r' && *end != '\n')
    end++;
  while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
    end--;
  *end = '\0';
  return start;
}

bool path_next_entry(const char **cursor, char sep, char *out, size_t out_cap) {
  const char *p = cursor ? *cursor : NULL;
  while (p && *p) {
    const char *end = strchr(p, sep);
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0 && len < out_cap) {
      memcpy(out, p, len);
      out[len] = '\0';
      *cursor = end ? end + 1 : NULL;
      return true;
    }
    if (!end) {
      *cursor = NULL;
      return false;
    }
    p = end + 1;
  }
  *cursor = NULL;
  return false;
}

bool owner_purge_inactive(const char *path_a, const char *path_b) {
  return (!path_a || !path_a[0]) && (!path_b || !path_b[0]);
}
