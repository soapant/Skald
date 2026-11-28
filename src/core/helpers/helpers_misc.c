#include "config_values.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_input_state.h"
#include "core/context.h"
#include "core/lua_host.h"
#include "core/types/keys.h"
#include "core/window_manager.h"
#include "core/feature_path.h"
#include "core/search_index.h"
#include "core/hotkey_manager.h"
#include "core/editbox.h"
#include "core/prompt_keys.h"
#include "platform/platform.h"
#include "platform/lib.h"
#include "deps/log.h"
#include "core/owner_purge.h"

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

bool monitor_geometry_at(MonitorGeometryAt *argspec) {
  MonitorEntry mon[16];
  int n = monitor_fill(mon, 16);
  if (n <= 0 || argspec->index < 0 || argspec->index >= n ||
      !mon[argspec->index].w)
    return false;
  if (argspec->x)
    *argspec->x = mon[argspec->index].x;
  if (argspec->y)
    *argspec->y = mon[argspec->index].y;
  if (argspec->w)
    *argspec->w = mon[argspec->index].w;
  if (argspec->h)
    *argspec->h = mon[argspec->index].h;
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

OWNER_PURGE_REGISTER2(key_purge_interceptor_owner)

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
  const char *lua_owner = slua_current_owner();
  if (lua_owner && lua_owner[0]) {
    size_t n = strlen(lua_owner) + 1;
    char *copy = malloc(n);
    if (copy)
      memcpy(copy, lua_owner, n);
    return copy;
  }
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

static bool svg_read_number(const char **p, const char *end, int *out) {
  while (*p < end && isspace((unsigned char)**p))
    (*p)++;
  if (*p >= end)
    return false;
  char *stop = NULL;
  double value = strtod(*p, &stop);
  if (!stop || stop == *p || stop > end || value <= 0)
    return false;
  *p = stop;
  *out = (int)value;
  return *out > 0;
}

static bool svg_read_attr(const char *svg, const char *end, const char *name,
                          int *out) {
  const char *p = strstr(svg, name);
  if (!p || p >= end)
    return false;
  p += strlen(name);
  while (p < end && isspace((unsigned char)*p))
    p++;
  if (p >= end || *p != '=')
    return false;
  p++;
  while (p < end && isspace((unsigned char)*p))
    p++;
  if (p < end && (*p == '"' || *p == '\''))
    p++;
  return svg_read_number(&p, end, out);
}

bool svg_file_size(const char *path, int *out_w, int *out_h) {
  if (!path || !path[0] || !out_w || !out_h)
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long size = ftell(f);
  if (size <= 0) {
    fclose(f);
    return false;
  }
  rewind(f);
  char *buf = (char *)malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t n = fread(buf, 1, (size_t)size, f);
  buf[n] = '\0';
  fclose(f);
  const char *end = buf + n;
  const char *svg = strstr(buf, "<svg");
  if (!svg || svg >= end) {
    free(buf);
    return false;
  }
  int w = 0, h = 0;
  bool have_w = svg_read_attr(svg, end, "width", &w);
  bool have_h = svg_read_attr(svg, end, "height", &h);
  if (!have_w || !have_h) {
    const char *vb = strstr(svg, "viewBox");
    if (vb && vb < end) {
      const char *p = strchr(vb, '=');
      if (p && p < end) {
        p++;
        int x = 0, y = 0;
        if (svg_read_number(&p, end, &x) && svg_read_number(&p, end, &y) &&
            svg_read_number(&p, end, &w) && svg_read_number(&p, end, &h)) {
          have_w = true;
          have_h = true;
        }
      }
    }
  }
  free(buf);
  if (!have_w || !have_h || w <= 0 || h <= 0)
    return false;
  *out_w = w;
  *out_h = h;
  return true;
}

char *file_read_all_alloc(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t got = fread(buf, 1, (size_t)sz, f);
  buf[got] = '\0';
  fclose(f);
  return buf;
}

void resolve_window_list(const char *const *wl, const char *const *bl,
                         const char *const **out_windows, bool *out_whitelist) {
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char *const *windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  if (out_windows)
    *out_windows = windows;
  if (out_whitelist)
    *out_whitelist = whitelist;
}

bool setting_parse_bool(const char *v, bool *out) {
  if (!v || !out)
    return false;
  if (lookup_ci_eq(v, "true") || lookup_ci_eq(v, "1") ||
      lookup_ci_eq(v, "yes") || lookup_ci_eq(v, "on")) {
    *out = true;
    return true;
  }
  if (lookup_ci_eq(v, "false") || lookup_ci_eq(v, "0") ||
      lookup_ci_eq(v, "no") || lookup_ci_eq(v, "off")) {
    *out = false;
    return true;
  }
  return false;
}

bool setting_parse_float(const char *v, float *out) {
  if (!v || !*v || !out)
    return false;
  char *end = NULL;
  float f = strtof(v, &end);
  if (!end || *end != '\0')
    return false;
  *out = f;
  return true;
}

bool setting_parse_int(const char *v, int *out) {
  if (!v || !*v || !out)
    return false;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (!end || *end != '\0' || n < INT_MIN || n > INT_MAX)
    return false;
  *out = (int)n;
  return true;
}

int config_index_of(ConfigIndexOf *argspec) {
  (void)argspec->key_size;
  if (!argspec->entries || !argspec->key)
    return -1;
  for (size_t i = 0; i < argspec->count; i++) {
    const char *field = (const char *)argspec->entries +
                        i * argspec->elem_size + argspec->key_off;
    if (lookup_ci_eq(field, argspec->key))
      return (int)i;
  }
  return -1;
}

bool canonical_path(const char *path, char *out, size_t cap) {
  if (!path || !out || cap == 0)
    return false;
#if defined(_WIN32)
  return _fullpath(out, path, cap) != NULL;
#else
  char *resolved = realpath(path, NULL);
  if (!resolved)
    return false;
  safe_copy(out, cap, resolved);
  free(resolved);
  return true;
#endif
}

bool paths_equal(const char *a, const char *b) {
#if defined(_WIN32)
  return _stricmp(a, b) == 0;
#else
  return strcmp(a, b) == 0;
#endif
}

bool rel_to_abs(Pos pos, Pos *out) {
  if (!out)
    return false;
  int vx = 0, vy = 0, vw = 0, vh = 0;
  if (!Platform_GetVirtualBounds(&vx, &vy, &vw, &vh))
    return false;
  out->x = vx + pos.x;
  out->y = vy + pos.y;
  return true;
}

bool pixel_color_matches_tol(RgbColor c, RgbColor target, int tolerance) {
  int tol = tolerance > 0 ? tolerance : 0;
  int dr = (int)c.r - (int)target.r;
  if (dr < 0)
    dr = -dr;
  if (dr > tol)
    return false;
  int dg = (int)c.g - (int)target.g;
  if (dg < 0)
    dg = -dg;
  if (dg > tol)
    return false;
  int db = (int)c.b - (int)target.b;
  if (db < 0)
    db = -db;
  return db <= tol;
}

bool feature_shortcut_parse(FeatureShortcutParse *argspec) {
  if (!argspec->target || !argspec->feature || !argspec->filter)
    return false;
  const char *first = strchr(argspec->target, '|');
  if (!first)
    return false;
  const char *second = strchr(first + 1, '|');
  const char *feature_start = second ? first + 1 : argspec->target;
  const char *filter_start = second ? second + 1 : first + 1;
  size_t feature_len = second ? (size_t)(second - feature_start)
                              : (size_t)(first - argspec->target);
  size_t filter_len = strlen(filter_start);
  if (!feature_len || !filter_len)
    return false;
  if (feature_len >= argspec->feature_cap || filter_len >= argspec->filter_cap)
    return false;
  memcpy(argspec->feature, feature_start, feature_len);
  argspec->feature[feature_len] = '\0';
  memcpy(argspec->filter, filter_start, filter_len);
  argspec->filter[filter_len] = '\0';
  return true;
}

void feature_shortcut_select(ListBox *lb, const char *filter) {
  if (!lb || !filter || !filter[0])
    return;
  int fallback = -1;
  for (int i = 0; i < lb->item_count; i++) {
    const char *row = listbox_info_row(lb, i);
    if (!row || !row[0])
      continue;
    if (strcmp(row, filter) == 0) {
      UI_SetSelection(lb, i);
      return;
    }
    if (fallback < 0 && strstr(row, filter))
      fallback = i;
  }
  if (fallback >= 0)
    UI_SetSelection(lb, fallback);
}

PromptCallbacks feature_shortcut_make_callbacks(const PromptSpec *spec,
                                                const PromptCallbacks *saved) {
  PromptCallbacks cb = *saved;
  if (spec->refresh)
    cb.refresh = spec->refresh;
  if (spec->enter)
    cb.enter = spec->enter;
  if (spec->ctrl_enter)
    cb.ctrl_enter = spec->ctrl_enter;
  if (spec->shift_enter)
    cb.shift_enter = spec->shift_enter;
  if (spec->ctrl_shift_enter)
    cb.ctrl_shift_enter = spec->ctrl_shift_enter;
  if (spec->ctrl_alt_enter)
    cb.ctrl_alt_enter = spec->ctrl_alt_enter;
  if (spec->shift_alt_enter)
    cb.shift_alt_enter = spec->shift_alt_enter;
  if (spec->render)
    cb.render = spec->render;
  if (spec->edit)
    cb.edit = spec->edit;
  if (spec->filter)
    cb.filter = spec->filter;
  if (spec->help)
    cb.help = spec->help;
  return cb;
}

PromptState *current_prompt_state(void) {
  SkaldContext *ctx = SkaldContext_Get();
  return (ctx && ctx->window.headless.state)
             ? ctx->window.headless.state
             : prompt_popup_get_top_prompt_state();
}

unsigned long draw_color(unsigned long color) {
  return rgb_to_hex((color == 0 || color == COLOR_AUTO) ? COLOR_TEXT : color);
}

bool resolve_caller_path_silent(void *caller_ret, const char *name, char *full,
                                size_t cap) {
  return feature_path_resolve_data_path(caller_ret, name, full, cap);
}

bool resolve_caller_path(ResolveCallerPath *argspec) {
  if (!resolve_caller_path_silent(argspec->caller_ret, argspec->name,
                                  argspec->full, argspec->cap)) {
    log_warn("%s resolve failed name='%s' feature='%s'", argspec->func,
             argspec->name,
             SkaldContext_Get()->window.current_feature
                 ? SkaldContext_Get()->window.current_feature
                 : "(null)");
    return false;
  }
  return true;
}

bool resolve_icon_path(void *caller, const char *icon, char *out, size_t cap) {
  if (!icon || (icon[0] != '/' && icon[0] != '\\') || !out || cap == 0)
    return false;
  char asset[MAX_PATH];
  const char *owner = slua_current_owner();
  if (owner &&
      feature_path_resolve_asset_for_owner(owner, icon, asset, sizeof(asset)))
    return safe_copy(out, cap, asset);
  if (feature_path_resolve_asset_path(caller, icon, asset, sizeof(asset)))
    return safe_copy(out, cap, asset);
  return false;
}

StringList load_file_list_from(void *caller, const char *filepath) {
  StringList list = {0};
  if (!filepath)
    return list;
  char full[MAX_PATH];
  if (!resolve_caller_path(&(ResolveCallerPath){.caller_ret = caller,
                                                .name = filepath,
                                                .full = full,
                                                .cap = sizeof(full),
                                                .func = "sk_load_file_list"}))
    return list;
  list = File_ReadLines(full);
  return list;
}

bool write_lines_from(void *caller, const char *filepath, const char *lines[],
                      int count) {
  if (!filepath)
    return false;
  if (count < 0)
    return false;
  if (count > 0 && !lines)
    return false;
  char full[MAX_PATH];
  if (!resolve_caller_path_silent(caller, filepath, full, sizeof(full)))
    return false;
  FILE *f = fopen(full, "w");
  if (!f)
    return false;
  for (int i = 0; i < count; i++) {
    if (lines[i])
      fprintf(f, "%s\n", lines[i]);
    else
      fprintf(f, "\n");
  }
  fclose(f);
  return true;
}

bool filter_lines_from(void *caller, const char *path, const char *needle,
                       bool match_substr) {
  StringList files = load_file_list_from(caller, path);
  const char **kept =
      (const char **)malloc(sizeof(char *) * (files.count ? files.count : 1));
  if (!kept) {
    string_list_free(&files);
    return false;
  }
  int k = 0;
  for (size_t i = 0; i < files.count; i++) {
    bool drop = match_substr ? strstr(files.items[i], needle) != NULL
                             : strcmp(files.items[i], needle) == 0;
    if (!drop)
      kept[k++] = files.items[i];
    else {
      free(files.items[i]);
      files.items[i] = NULL;
    }
  }
  bool ok = write_lines_from(caller, path, kept, k);
  free(kept);
  string_list_free(&files);
  return ok;
}

bool type_text_common(const char *s) {
  if (!s)
    return false;
  char *old = Platform_GetClipboardEntry();
  Platform_CopyToClipboard(s);
  Platform_EmitKey(VK_LCONTROL, 1);
  Platform_EmitKey('V', 1);
  Platform_EmitKey('V', 0);
  Platform_EmitKey(VK_LCONTROL, 0);
  WindowManager *wm = WindowManager_Get();
  for (int i = 0; i < 40; i++) {
    if (wm)
      WindowManager_ProcessEvents(wm);
    Platform_SleepSeconds(0.005);
  }
  if (old) {
    Platform_CopyToClipboard(old);
    free(old);
  }
  return true;
}

ImageBuffer image_take(ImageBuffer *img) {
  ImageBuffer out = img ? *img : (ImageBuffer){0};
  Platform_FreeImage(img);
  free(img);
  return out;
}

bool draw_rect_raw(DrawRectRaw *argspec) {
  return Platform_DrawRectRaw((DrawRectRawSpec){
      .x = argspec->spec->x,
      .y = argspec->spec->y,
      .w = argspec->spec->w,
      .h = argspec->spec->h,
      .color = argspec->color,
      .radius = argspec->spec->radius ? argspec->spec->radius : 0,
      .rounded =
          argspec->spec->rounded ? argspec->spec->rounded : RoundedCorners,
      .opaque = !argspec->transparent,
      .thickness = argspec->thickness,
      .filled = argspec->filled,
      .angle = argspec->spec->angle});
}

bool place_editbox(const EditSpec *spec) {
  if (!spec) {
    return false;
  }
  if (search_index_current()) {
    const char *name = (spec->name && spec->name[0])
                           ? spec->name
                           : PROMPT_PRIMARY_EDITBOX_NAME;
    if (strncmp(name, "settings-search:", 16) != 0)
      search_index_add_text(&(SearchIndexAddText){.idx = NULL,
                                                  .text = spec->buffer,
                                                  .kind = SEARCH_KIND_EDITBOX,
                                                  .widget_name = name,
                                                  .value = NULL});
    return true;
  }
  SkaldContext_Get()->window.editbox_placed = true;
  EditSpec copy = *spec;
  copy.hide_caret = false;
  PromptState *state = current_prompt_state();
  const char *name = spec->name;
  char *buffer = spec->buffer;
  size_t buffer_size = spec->buffer_size;
  bool primary = spec->primary || !name || !name[0];
  if (primary && copy.w <= 0 && state)
    copy.w = state->width;
  if (state && primary && !buffer && state->default_eb) {
    buffer = state->default_eb->filter;
    buffer_size = sizeof(state->default_eb->filter);
  }
  if (state && (!name || !name[0]))
    name = PROMPT_PRIMARY_EDITBOX_NAME;
  bool fresh = state && name && name[0] && !editbox_registry_find(state, name);
  if (state && name && name[0]) {
    int dx = 0, dy = 0;
    get_draw_offset(&dx, &dy);
    int idx = editbox_registry_place(
        &(EditboxRegistryPlace){.state = state,
                                .name = name,
                                .buffer = buffer,
                                .buffer_size = buffer_size,
                                .x = spec->x + dx,
                                .y = spec->y + dy,
                                .w = copy.w,
                                .h = spec->h});
    if (idx >= 0) {
      PromptEditbox *entry = &state->editboxes[idx];
      if (state->active_editbox < 0)
        state->active_editbox = idx;
      entry->primary = primary;
      entry->no_tab = spec->no_tab;
      entry->wrapped = spec->wrapped;
      entry->font_size = spec->font_size > 0 ? spec->font_size : FontSize;
      if (fresh && spec->auto_focus)
        editbox_registry_set_active(state, name);
      const char *ph = copy.placeholder;
      if (primary && (!ph || !ph[0]) && state->default_eb)
        ph = state->default_eb->placeholder;
      if (ph && ph[0]) {
        if (!entry->eb.placeholder || strcmp(entry->eb.placeholder, ph) != 0) {
          free(entry->eb.placeholder);
          entry->eb.placeholder = strdup(ph);
        }
      } else {
        free(entry->eb.placeholder);
        entry->eb.placeholder = NULL;
      }
      copy.placeholder = entry->eb.placeholder;
      if (fresh && copy.initial_text && copy.initial_text[0]) {
        size_t cap =
            buffer && buffer_size ? buffer_size : sizeof(entry->eb.filter);
        size_t n = strnlen(copy.initial_text, cap - 1);
        if (n >= sizeof(entry->eb.filter))
          n = sizeof(entry->eb.filter) - 1;
        memcpy(entry->eb.filter, copy.initial_text, n);
        entry->eb.filter[n] = '\0';
        entry->eb.cursor = (int)n;
        if (buffer && buffer_size) {
          memcpy(buffer, copy.initial_text, n);
          buffer[n] = '\0';
        }
      }
      copy.buffer = entry->eb.filter;
      copy.hide_caret = state->active_editbox != idx;
      if (state->active_editbox == idx) {
        state->eb = &entry->eb;
      } else {
        EditBox tmp;
        memset(&tmp, 0, sizeof(tmp));
        safe_copy(tmp.filter, sizeof(tmp.filter), entry->eb.filter);
        tmp.cursor = strlen(tmp.filter);
        EditBox *saved = state->eb;
        int saved_active = state->active_editbox;
        state->eb = &tmp;
        state->active_editbox = idx;
        bool ok = Platform_PlaceEditbox(&copy);
        state->active_editbox = saved_active;
        state->eb = saved;
        return ok;
      }
    }
  }
  return Platform_PlaceEditbox(&copy);
}
