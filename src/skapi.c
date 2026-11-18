#include <stdint.h>
#include "helpers/helpers_fs.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/checkbox.h"
#include "core/types/file.h"
#include "core/types/prompt.h"
#include "core/widget_registry.h"
#include "deps/log.h"
#include "config.h"
#include "core/config_toml.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_input_state.h"
#include "core/window_manager.h"
#include "core/theme.h"
#include "core/thread_registry.h"
#include "core/http.h"
#include "core/owner_purge.h"
#include "platform/lib.h"
#include "core/clipboard_history.h"
#include "core/skapi.h"
#include "core/config_section.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "core/feature_loader.h"
#include "core/feature_path.h"
#include "core/cap_registry.h"
#include "helpers/helpers_owner.h"
#include "platform/widget.h"
#include "core/hover_widget.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "helpers/helpers_render_thread.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include "deps/stb_ds.h"
#pragma GCC diagnostic pop
#include "config_values.h"
#include "core/cc_registry.h"
#include "core/settings_registry.h"
#include "core/skapi_input.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/skapi_types.h"
#include "core/types/keys.h"
#include "core/types/window.h"

size_t sk_clipboard_history_count(void) {
  return clipboard_history_count_platform();
}

const SkClipboardEntry *sk_clipboard_history_get(size_t index) {
  return clipboard_history_get_platform(index);
}

const SkClipboardEntry *sk_clipboard_latest(void) {

  size_t n = clipboard_history_count_platform();
  if (n == 0)
    return NULL;
  return clipboard_history_get_platform(n - 1);
}

void sk_clipboard_clear_history(void) { clipboard_clear_history_platform(); }

bool sk_copy_to_clipboard(const char *text) {
  if (text == NULL)
    return 0;
  bool ok = Platform_CopyToClipboard(text);
  if (ok)
    clipboard_history_push_text_platform(text);
  return ok;
}

bool sk_copy_file_path(const char *path) {
  if (!path)
    return false;
  return clipboard_set_path_platform(path);
}

bool ski_dir_scan(const SkDirScanSpec *spec) {

  if (!spec || !spec->dir_path || !spec->out)
    return false;

  char expanded_dir[MAX_PATH];
  if (!expand_path(spec->dir_path, expanded_dir, sizeof(expanded_dir)))
    return false;
  const char *dir_path = expanded_dir;
  return file_walk_all_files_from_dir(dir_path, spec->allowed_extensions,
                                      spec->out, spec->names_to_ignore,
                                      spec->strip_extension, spec->max_depth);
}

char *sk_search_file_in_dir(const char *directory, const char *search_string) {

  if (!directory)
    return NULL;

  char expanded_dir[MAX_PATH];
  if (!expand_path(directory, expanded_dir, sizeof(expanded_dir)))
    return NULL;
  directory = expanded_dir;
  return sk_search_file_in_dir_platform(directory, search_string);
}

bool sk_launch_with_args(const char *path, const char *args) {
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded))) {
    return false;
  }
  return sk_launch_with_args_platform(expanded, args);
}

bool sk_launch_with_args_as_admin(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded)))
    return false;
  return sk_launch_with_args_as_admin_platform(expanded, args);
}

static bool cc_add_dep(const char *dep_name, const char *owner_file) {
  if (!dep_name || !dep_name[0] || !owner_file || !owner_file[0])
    return false;
  char dep[64];
  snprintf(dep, sizeof(dep), "%s", dep_name);
  char *dot = strrchr(dep, '.');
  if (dot)
    *dot = '\0';
  if (!dep[0])
    return false;
  feature_loader_manifest_add_depends(owner_file, dep);
  return true;
}

bool ski_add_dep(const char *dep_name, const char *owner_file) {
  return cc_add_dep(dep_name, owner_file);
}

unsigned sk_cap_generation(void) { return cap_generation(); }
void ski_cap_register(const char *key, void *fn) {
  cap_register_abi(key, CAP_ABI_V1, fn);
}
void *ski_cap_resolve(const char *key) {
  return cap_resolve_abi(key, CAP_ABI_V1);
}
void sk_cap_register_abi(const char *key, unsigned abi, void *fn) {
  cap_register_abi(key, abi, (void *)fn);
}
void *sk_cap_resolve_abi(const char *key, unsigned abi) {
  return cap_resolve_abi(key, abi);
}
void sk_cap_bind_declare(const char *key, void *consumer) {
  cap_bind_declare(key, consumer);
}

static bool sk_resolve_caller_path(void *caller_ret, const char *name,
                                   char *full, size_t cap, const char *func) {
  if (!feature_path_resolve_data_path(caller_ret, name, full, cap)) {
    log_warn("%s resolve failed name='%s' feature='%s'", func, name,
             SkaldContext_Get()->window.current_feature
                 ? SkaldContext_Get()->window.current_feature
                 : "(null)");
    return false;
  }
  return true;
}

static bool sk_resolve_caller_path_silent(void *caller_ret, const char *name,
                                          char *full, size_t cap) {
  return feature_path_resolve_data_path(caller_ret, name, full, cap);
}

SkStringList sk_load_file_list(const char *filepath) {
  SkStringList list = {0};
  if (!filepath)
    return list;
  char full[MAX_PATH];
  if (!sk_resolve_caller_path(__builtin_return_address(0), filepath, full,
                              sizeof(full), "sk_load_file_list"))
    return list;
  list = File_ReadLines(full);
  return list;
}

static char *file_read_all_alloc(const char *path) {
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

char *sk_load_file(const char *filepath) {
  if (!filepath)
    return NULL;
  char full[MAX_PATH];
  if (!sk_resolve_caller_path(__builtin_return_address(0), filepath, full,
                              sizeof(full), "sk_load_file"))
    return NULL;
  return file_read_all_alloc(full);
}

bool sk_free_list(const char *list[], int count) {
  if (!list || count <= 0)
    return false;
  for (int i = 0; i < count; i++)
    free((void *)list[i]);
  return true;
}

void sk_free_string_list(SkStringList *list) {
  if (!list) {
    return;
  }
  if (list->items) {
    for (size_t i = 0; i < list->count; i++)
      free(list->items[i]);
    free(list->items);
    list->items = NULL;
  }
  list->count = 0;
}

void sk_free_files_list(ListOfFiles *list) {
  if (!list) {
    return;
  }
  free(list->files);
  list->files = NULL;
  list->count = 0;
  list->capacity = 0;
}
bool sk_free_window_list(SkWindowList *list) {
  if (!list)
    return false;
  if (list->windows) {
    for (int i = 0; i < list->count; i++)
      free(list->windows[i]);
    free(list->windows);
    list->windows = NULL;
    list->count = 0;
  }
  return true;
}

bool sk_append_to_file(const char *filepath, const char *line) {
  if (!filepath || !line)
    return false;
  char full[MAX_PATH];
  if (!sk_resolve_caller_path_silent(__builtin_return_address(0), filepath,
                                     full, sizeof(full)))
    return false;
  filepath = full;
  FILE *f = fopen(filepath, "a");
  if (!f) {

    f = fopen(filepath, "w");
    if (!f)
      return false;
  }
  fclose(f);
  f = fopen(filepath, "r");
  bool need_newline = false;
  if (f) {
    if (fseek(f, 0, SEEK_END) == 0) {
      long sz = ftell(f);
      if (sz > 0) {
        if (fseek(f, -1, SEEK_END) == 0) {
          int c = fgetc(f);
          if (c != '\n' && c != '\r')
            need_newline = true;
        }
      }
    }
    fclose(f);
  }
  f = fopen(filepath, "a");
  if (!f)
    return false;
  if (need_newline)
    fputc('\n', f);
  fputs(line, f);
  fputc('\n', f);
  fclose(f);
  return true;
}

bool sk_write_to_file(const char *filepath, const char *lines[], int count) {
  if (!filepath)
    return false;
  if (count < 0)
    return false;
  if (count > 0 && !lines)
    return false;
  char full[MAX_PATH];
  if (!sk_resolve_caller_path_silent(__builtin_return_address(0), filepath,
                                     full, sizeof(full)))
    return false;
  filepath = full;
  FILE *f = fopen(filepath, "w");
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

char *sk_write_temp_file(const char *name, const void *data, size_t len) {
  char path[2048];
  if (!temp_file_write(name, data, len, path, sizeof(path)))
    return NULL;
  return strdup(path);
}

char *sk_read_file_to_string(const char *path, size_t *out_len) {
  if (!path)
    return NULL;
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded)))
    safe_copy(expanded, sizeof(expanded), path);
  FILE *f = fopen(expanded, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    {
      return NULL;
    }
  }
  long sz = ftell(f);
  if (sz < 0)
    sz = 0;
  rewind(f);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    {
      return NULL;
    }
  }
  size_t read = fread(buf, 1, (size_t)sz, f);
  buf[read] = '\0';
  fclose(f);
  if (out_len)
    *out_len = read;
  return buf;
}

char *sk_read_entire_file(const char *full_path) {
  if (!full_path)
    return NULL;
  return file_read_all_alloc(full_path);
}

bool sk_write_string_to_file(const char *path, const char *content) {
  if (!path || !content)
    return false;
  char full[MAX_PATH];
  if (feature_path_resolve_data_path(__builtin_return_address(0), path, full,
                                     sizeof(full))) {
    path = full;
  } else {
    char expanded[MAX_PATH];
    if (!expand_path(path, expanded, sizeof(expanded)))
      safe_copy(expanded, sizeof(expanded), path);
    path = expanded;
  }
  FILE *f = fopen(path, "w");
  if (!f)
    return false;
  fputs(content, f);
  fclose(f);
  return true;
}

bool sk_file_exists(const char *path) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded)))
    safe_copy(expanded, sizeof(expanded), path);
  FILE *f = fopen(expanded, "rb");
  if (f) {
    fclose(f);
    return true;
  }
  return false;
}

bool sk_append_unique_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  SkStringList files = sk_load_file_list(path);
  for (size_t i = 0; i < files.count; i++)
    if (files.items[i] && strcmp(files.items[i], line) == 0) {
      sk_free_string_list(&files);
      return true;
    }
  bool ok = sk_append_to_file(path, line);
  sk_free_string_list(&files);
  return ok;
}

static bool filter_lines(const char *path, const char *needle,
                         bool match_substr) {
  SkStringList files = sk_load_file_list(path);
  const char **kept =
      (const char **)malloc(sizeof(char *) * (files.count ? files.count : 1));
  if (!kept) {
    sk_free_string_list(&files);
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
  bool ok = sk_write_to_file(path, kept, k);
  free(kept);
  sk_free_string_list(&files);
  return ok;
}

bool sk_remove_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  return filter_lines(path, line, false);
}

bool sk_remove_line_contains(const char *path, const char *substr) {
  if (!path || !substr)
    return false;
  return filter_lines(path, substr, true);
}

char *sk_parse_desktop_file(const char *path, bool *out_nodisplay) {

  if (!path)
    return NULL;
  char name_buf[512] = {0};
  char *out_name = name_buf;
  size_t n = sizeof(name_buf);
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded)))
    safe_copy(expanded, sizeof(expanded), path);
  FILE *f = fopen(expanded, "r");
  if (!f)
    return NULL;
  char line[1024];
  bool found = false;
  if (out_nodisplay)
    *out_nodisplay = false;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (string_contains_ci(p, "NoDisplay=true") ||
        string_contains_ci(p, "Hidden=true")) {
      if (out_nodisplay)
        *out_nodisplay = true;
    }
    if (!found && string_starts_with_ci(p, "Name=")) {
      p += 5;
      while (*p == ' ' || *p == '\t')
        p++;
      p[strcspn(p, "\r\n")] = '\0';
      safe_copy(out_name, n, p);
      out_name[n - 1] = '\0';
      found = true;
    }
  }
  fclose(f);
  return found ? strdup(name_buf) : NULL;
}

static void resolve_window_list(const char *const *wl, const char *const *bl,
                                const char *const **out_windows,
                                bool *out_whitelist) {
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char *const *windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  if (out_windows)
    *out_windows = windows;
  if (out_whitelist)
    *out_whitelist = whitelist;
}

bool ski_register_remapping(const SkRemappingSpec *spec) {
  if (!spec || !spec->src || !spec->dst)
    return false;
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  const char *const *windows;
  bool whitelist;
  resolve_window_list(wl, bl, &windows, &whitelist);
  return remap_register_internal(spec->src, spec->dst, whitelist, windows);
}

static void snippet_fire(int idx, int del) {

  const SnippetBinding *b = snippet_get_binding(idx);
  if (!b || !b->trigger) {
    return;
  }
  // Hackish but snippets are only a few characters
  for (int i = 0; i < del; i++) {
    Platform_EmitKey(VK_BACK, 1);
    Platform_EmitKey(VK_BACK, 0);
  }
  if (b->text)
    sk_type_text(b->text);
  else if (b->func)
    b->func();
}

bool ski_register_snippet(const SkSnippetSpec *spec) {
  if (!spec || !spec->trigger || (!spec->text && !spec->fn))
    return false;
  snippet_set_fire_fn(snippet_fire);
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  const char *const *windows;
  bool whitelist;
  resolve_window_list(wl, bl, &windows, &whitelist);
  return snippet_register_internal(spec->trigger, spec->text, spec->fn,
                                   whitelist, windows,
                                   (void *)__builtin_return_address(0));
}

bool ski_unregister_snippet(const char *trigger) {
  return snippet_unregister_internal(trigger);
}

typedef struct {
  char name[128];
  void (*cb)(void);
  char icon[512];
  char owner[OWNER_PATH_MAX];
} FeatureReg;

static FeatureReg **feature_order = NULL;
static int feature_order_count = 0;
static int feature_order_cap = 0;

static int feature_list_find(const char *name, void (*cb)(void)) {
  for (int i = 0; i < feature_order_count; i++) {
    if (cb && feature_order[i]->cb == cb)
      return i;
    if (name && feature_order[i]->name[0] &&
        strcmp(feature_order[i]->name, name) == 0)
      return i;
  }
  return -1;
}

static bool feature_list_add(const char *name, void (*cb)(void),
                             const char *icon, const char *owner) {
  if (!name || !name[0])
    return false;
  int idx = feature_list_find(name, cb);
  if (idx < 0) {
    if (feature_order_count >= feature_order_cap) {
      int ncap = feature_order_cap ? feature_order_cap * 2 : 32;
      FeatureReg **grown =
          (FeatureReg **)realloc(feature_order, (size_t)ncap * sizeof(*grown));
      if (!grown)
        return false;
      feature_order = grown;
      feature_order_cap = ncap;
    }
    idx = feature_order_count;
    feature_order[idx] = (FeatureReg *)calloc(1, sizeof(**feature_order));
    if (!feature_order[idx])
      return false;
    feature_order_count++;
  }
  FeatureReg *reg = feature_order[idx];
  snprintf(reg->name, sizeof(reg->name), "%s", name);
  reg->cb = cb;
  snprintf(reg->icon, sizeof(reg->icon), "%s", icon ? icon : "");
  snprintf(reg->owner, sizeof(reg->owner), "%s", owner ? owner : "");
  return true;
}

void feature_list_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  int n = 0;
  for (int i = 0; i < feature_order_count; i++) {
    if (owner_module_matches(feature_order[i]->owner, path_a, path_b)) {
      free(feature_order[i]);
      continue;
    }
    feature_order[n++] = feature_order[i];
  }
  feature_order_count = n;
}

bool ski_register_hotkey(const SkHotkeySpec *spec) {
  if (!spec || !spec->hotkey || (!spec->cb && !spec->cb_ud))
    return false;
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  const char *const *bw;
  bool whitelist;
  resolve_window_list(wl, bl, &bw, &whitelist);
  HotkeyBinding binding = hotkey_convert_into_binding(
      (char *)spec->hotkey, spec->cb, NULL, bw, whitelist, spec->ud);
  binding.hotkey_callback_ud = spec->cb_ud;
  binding.hold = false;
  binding.should_swallow = spec->should_swallow;
  return hotkey_register_internal(binding);
}

bool ski_unregister_hotkey(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyBinding binding = hotkey_convert_into_binding((char *)hotkey, NULL,
                                                      NULL, NULL, false, NULL);
  return hotkey_unregister_internal(binding.vk_code, binding.modifiers);
}

void sk_disable_global_hotkeys(void) { hotkey_set_suspended(true); }

void sk_enable_global_hotkeys(void) { hotkey_set_suspended(false); }

int sk_hotkey_count(void) { return hotkey_count(); }

static char *hotkey_binding_format(const HotkeyBinding *b) {
  if (!b)
    return NULL;
  HotkeyKey key = {.vk_code = b->vk_code, .mods = b->modifiers};
  char tmp[256];
  if (!hotkey_string_format(key, tmp, (int)sizeof(tmp)))
    return NULL;
  return strdup(tmp);
}

char *sk_hotkey_string_at(int index) {
  return hotkey_binding_format(hotkey_get(index));
}

char *sk_hotkey_for_ud(void *ud) {
  return hotkey_binding_format(hotkey_find_by_ud(ud));
}

void sk_register_deferred_init(void (*cb)(void)) { deferred_init_register(cb); }

char *sk_hotkey_for_callback(void (*cb)(void)) {
  return hotkey_binding_format(hotkey_find_by_callback((HotkeyCallback)cb));
}

bool sk_hotkey_taken(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyKey key = {0};
  if (!hotkey_string_parse(hotkey, &key))
    return false;
  return hotkey_is_taken(key.vk_code, key.mods);
}

SkFeatureList sk_get_feature_list(void) {
  SkFeatureList list = {NULL, 0};
  if (feature_order_count <= 0)
    return list;
  SkFeature *items =
      (SkFeature *)malloc((size_t)feature_order_count * sizeof(*items));
  if (!items)
    return list;
  for (int i = 0; i < feature_order_count; i++) {
    items[i].name = feature_order[i]->name;
    items[i].callback = feature_order[i]->cb;
    items[i].icon = feature_order[i]->icon[0] ? feature_order[i]->icon : NULL;
  }
  list.items = items;
  list.count = (size_t)feature_order_count;
  return list;
}

void sk_free_feature_list(SkFeatureList *list) {
  if (!list)
    return;
  free(list->items);
  list->items = NULL;
  list->count = 0;
}

char *sk_get_icon_path(const char *path) {
  if (!path)
    return NULL;
  return Platform_GetIconPath((char *)path);
}

bool sk_prewarm_image(const char *path) {
  if (!path || !path[0])
    return false;
  return Platform_PrewarmImage((char *)path);
}

int sk_get_icon_size(const char *icon_path) {
  if (!icon_path)
    return 0;
  return Platform_GetIconSize((char *)icon_path);
}

char *sk_run_terminal_command(const char *command) {
  if (!command) {
    return NULL;
  }
  return Platform_RunTerminalCommand(command, false);
}

char *sk_run_argv(const char *const argv[], const char *stdin_data,
                  int *out_status) {

  if (!argv || !argv[0])
    return NULL;
  return Platform_RunArgvCapture(argv, stdin_data, out_status);
}

char *sk_which(const char *command) {

  if (!command || !command[0])
    return NULL;
  return Platform_FindExecutable(command);
}

char *sk_run_terminal_command_blocking(const char *command) {
  if (!command)
    return NULL;
  return Platform_RunCommandAndGetOutput(command);
}

char *sk_run_terminal_command_and_open(const char *command) {
  if (!command) {
    return NULL;
  }
  return Platform_RunTerminalCommand(command, true);
}

bool sk_launch_detached(const char *command) {
  if (!command) {
    return false;
  }
  return Platform_LaunchDetached(command);
}

bool sk_launch_terminal_at_path(const char *path, const char *args) {
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!expand_path(path, expanded, sizeof(expanded))) {
    return false;
  }
  return Platform_LaunchTerminalAtPath(expanded, args);
}

bool sk_process_running(const char *name) {
  return Platform_ProcessRunning(name);
}

bool sk_terminate_process_by_name(const char *exe_name) {
  if (!exe_name)
    return false;
  return Platform_TerminateProcessByName(exe_name);
}
bool sk_string_contains(const char *haystack, const char *needle) {
  if (!needle || !haystack)
    return false;
  return string_contains(haystack, needle);
}

bool sk_string_contains_ci(const char *haystack, const char *needle) {
  if (!needle || !haystack)
    return false;
  return string_contains_ci(haystack, needle);
}

char *sk_string_cut_at(const char *str, char delim) {
  return string_cut_at(str, delim);
}
char *sk_string_cut_before(const char *str, char delim) {
  return string_cut_before(str, delim);
}
char *sk_string_cut_after(const char *str, char delim) {
  return string_cut_after(str, delim);
}
char *sk_string_between(const char *str, char open, char close) {
  return string_between(str, open, close);
}
char *sk_string_inside(const char *str, char delim) {
  return string_inside(str, delim);
}
char *sk_string_trim(const char *str) { return string_trim(str); }

char *sk_string_to_lower(const char *str) { return string_to_lower(str); }

char *sk_string_to_upper(const char *str) { return string_to_upper(str); }

bool sk_string_starts_with(const char *str, const char *prefix) {
  return string_starts_with(str, prefix);
}
bool sk_string_starts_with_ci(const char *str, const char *prefix) {
  return string_starts_with_ci(str, prefix);
}
bool sk_string_ends_with(const char *str, const char *suffix) {
  return string_ends_with(str, suffix);
}
bool sk_string_equals_ci(const char *str, const char *other) {
  return string_equals_ci(str, other);
}
char *sk_string_replace(const char *str, const char *from, const char *to) {
  return string_replace(str, from, to);
}
char *sk_string_slice(const char *str, int start, int end) {
  return string_slice(str, start, end);
}

StringList sk_string_split(const char *str, const char *delimiter) {
  StringList empty = {NULL, 0};
  if (!str || !delimiter || !delimiter[0])
    return empty;
  return String_Split(str, delimiter);
}

void sk_string_list_free(StringList *list) { string_list_free(list); }

void ski_populate_listbox(const SkPopulateListboxSpec *spec) {
  if (!spec)
    return;
  populate_listbox(spec->lb, spec->filter, spec->items, spec->count,
                   spec->filter_match);
}

bool sk_is_selected(ListBoxInfo *lb) {
  if (!lb)
    return false;
  return lb->selected_index != -1;
}

void sk_set_edit_text(const char *text) { UI_SetText(text ? text : ""); }

void sk_set_edit_cursor(int position) { UI_SetCursorPosition(position); }

int sk_get_edit_cursor(void) { return UI_GetCursorPosition(); }

char *sk_get_edit_text(void) {
  char tmp[8192];
  UI_GetCurrentTextEdit(tmp, sizeof(tmp));
  return strdup(tmp);
}

static void hover_popup_frame(void *ud) {
  SkHoverPopupCtx *p = (SkHoverPopupCtx *)ud;
  if (!p || !p->render) {
    return;
  }
  p->frame++;
  set_draw_offset(0, 0);
  begin_measure();
  p->render(p->render_data);
  int mw = 0, mh = 0;
  end_measure(&mw, &mh);
  if (mw > 0 && mh > 0) {
    p->w = mw + p->pad;
    p->h = mh + p->pad;
  }
}

static SkHoverPopupCtx *hover_popup_ctx_new(const SkMessageSpec *spec) {

  SkHoverPopupCtx *p = (SkHoverPopupCtx *)calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->render = spec->render;
  p->render_data = spec->render_data;
  p->w = spec->w > 0 ? spec->w : WIDGET_PROVISIONAL_W;
  p->h = spec->h > 0 ? spec->h : WIDGET_PROVISIONAL_H;
  p->created_at = sk_start_timer();
  p->pad = WIDGET_DEFAULT_PAD;
  return p;
}

static void hover_popup_ctx_free(void *ud) {
  SkHoverPopupCtx *p = (SkHoverPopupCtx *)ud;
  if (p)
    free(p);
}

#define SK_FMT_BUFFER()                                    \
  char buf[1024];                                        \
  va_list args;                                          \
  va_start(args, fmt);                                   \
  vsnprintf(buf, sizeof(buf), fmt, args);                \
  va_end(args)

void sk_message_fmt(const SkMessageSpec *spec, const char *fmt, ...) {
  SK_FMT_BUFFER();
  SkMessageSpec final;
  if (spec)
    final = *spec;
  else
    memset(&final, 0, sizeof(final));
  final.message = buf;
  ski_message(&final);
}

void ski_message(const SkMessageSpec *spec) {
  if (!MessagesEnabled)
    return;
  if (!spec)
    return;
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  int mw = spec->mw > 0 ? spec->mw : 0;
  int mh = spec->mh > 0 ? spec->mh : 0;
  double timeout =
      spec->timeout > 0 ? spec->timeout : (double)TimeOutForMessages;
  if (spec->close_on_keypress)
    timeout = MSG_TIMEOUT_INDEFINITE;
  ColorRef color = spec->color_text ? spec->color_text : COLOR_TEXT;
  if (spec->hover_key && spec->hover_key[0]) {
    if (spec->render) {
      SkHoverPopupCtx *ctx = hover_popup_ctx_new(spec);
      if (ctx) {
        hover_widget_show((HoverWidgetShowSpec){
            .key = spec->hover_key,
            .render = hover_popup_frame,
            .ud = ctx,
            .owner_src = (void *)spec->render,
            .x = spec->x,
            .y = spec->y,
            .w = ctx->w,
            .h = ctx->h,
            .fps = spec->fps > 0 ? spec->fps : 1,
            .no_bg = spec->no_bg,
            .bg_hex = spec->color_bg
                          ? rgb_to_hex(spec->color_bg)
                          : (spec->no_bg ? 0UL : rgb_to_hex(COLOR_BG)),
            .release = hover_popup_ctx_free,
            .release_ud = ctx});
      }
      return;
    }
    Platform_HoverMessage((HoverMessageSpec){.key = spec->hover_key,
                                             .message = spec->message,
                                             .x = spec->x,
                                             .y = spec->y,
                                             .w = w,
                                             .h = h,
                                             .max_width = mw,
                                             .max_height = mh,
                                             .color = color,
                                             .no_bg = spec->no_bg});
    return;
  }
  MessageSlot slot = spec->slot ? spec->slot : (spec->render ? FREE : AUTO);
  if (slot == AUTO) {
    slot = is_popup_active(WH_NULL) ? TOP_CENTER_RELATIVE : BOTTOM_CENTER;
  } else if (slot == FREE) {
    Platform_DisplayMessage(
        (DisplayMessageSpec){.message = spec->message,
                             .x = spec->x,
                             .y = spec->y,
                             .no_bg = spec->no_bg,
                             .w = w,
                             .h = h,
                             .max_width = mw,
                             .max_height = mh,
                             .timeout = timeout,
                             .color = color,
                             .render = spec->render ? spec->render : NULL,
                             .name = spec->name,
                             .render_data = spec->render_data,
                             .rounded = spec->rounded,
                             .radius = spec->radius,
                             .fps = spec->fps});
    return;
  }
  display_message_slot(&(MessageSlotSpec){.message = spec->message,
                                          .slot = slot,
                                          .max_w = mw,
                                          .max_h = mh,
                                          .prompt_gap = spec->prompt_gap,
                                          .timeout = timeout,
                                          .color = color,
                                          .render = spec->render,
                                          .render_data = spec->render_data,
                                          .name = spec->name,
                                          .win_w = spec->w,
                                          .win_h = spec->h,
                                          .rounded = spec->rounded,
                                          .radius = spec->radius,
                                          .fps = spec->fps});
}

bool sk_launch_browser(const char *url) {
  if (!url) {
    return false;
  }
  return Platform_LaunchBrowserAtUrl(url, false);
}

bool sk_query_browser(const char *query) {
  if (!query)
    return false;
  return Platform_QueryBrowser(query);
}

char *sk_get_last_clipboard_entry(void) {
  char *current = Platform_GetClipboardEntry();
  if (current) {
    if (current[0]) {
      const SkClipboardEntry *latest = sk_clipboard_latest();
      if (!latest || latest->type != CLIPBOARD_TEXT || !latest->text ||
          strcmp(latest->text, current) != 0)
        clipboard_history_push_text_platform(current);
    }
    free(current);
  }
  for (size_t i = clipboard_history_count_platform(); i > 0; i--) {
    const SkClipboardEntry *e = clipboard_history_get_platform(i - 1);
    if (e->type == CLIPBOARD_TEXT && e->text)
      return strdup(e->text);
  }
  return NULL;
}

bool sk_paste_from_clipboard(void) {
  char *content = Platform_GetClipboardEntry();
  if (!content)
    return false;
  bool ok = Platform_PasteContent(content);
  free(content);
  return ok;
}

bool sk_paste(const char *content) {
  if (!content)
    return false;
  return Platform_PasteContent(content);
}

// needed for returning of the buffer to function properly
static bool popup_enter(PromptState *state) {
  (void)state;
  return true;
}

char *ski_edit_text_popup(const SkEditPopupSpec *spec) {
  if (!spec)
    return NULL;
  const char *initial_text = spec->initial_text;
  int x = spec->x ? spec->x : 0;
  int y = spec->y ? spec->y : 0;
  int w = spec->w ? spec->w : 600;
  int h = spec->h ? spec->h : 50;
  ListBoxInfo *lb = spec ? spec->lb : NULL;
  int *out_index = spec ? spec->out_index : NULL;
  int idx = lb ? lb->selected_index : -1;
  int true_idx = (lb && idx >= 0 && idx < lb->item_count)
                     ? lb->matching_indices[idx]
                     : idx;
  if (out_index)
    *out_index = -1;
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  PromptSpec pspec = {0};
  pspec.initial_text = initial_text;
  pspec.enter = popup_enter;
  pspec.x = x;
  pspec.y = y;
  pspec.w = w;
  pspec.h = h;
  {
    SkaldContext *ctx = SkaldContext_Get();
    if (ctx)
      ctx->window.editbox_placed = false;
  }
  event_bus_trigger(EVENT_PROMPT_OPEN);
  char *r = prompt_user_run(wm, &pspec);
  if (r && out_index)
    *out_index = true_idx;
  if (lb && idx >= 0) {
    int keep_idx = idx < lb->item_count ? idx : lb->item_count - 1;
    if (keep_idx >= 0)
      lb->selected_index = keep_idx;
  }
  return r ? strdup(r) : NULL;
}

bool sk_replace_line_in_file(const char *filepath, int line_index,
                             const char *new_text) {
  if (!filepath || !new_text || line_index < 0)
    return false;
  SkStringList files = sk_load_file_list(filepath);
  if ((size_t)line_index >= files.count) {

    sk_free_string_list(&files);
    return false;
  }
  free(files.items[line_index]);
  files.items[line_index] = NULL;
  files.items[line_index] = strdup(new_text);
  if (!files.items[line_index]) {

    sk_free_string_list(&files);
    return false;
  }
  bool ok = sk_write_to_file(filepath, (const char **)files.items, files.count);
  sk_free_string_list(&files);
  return ok;
}

bool sk_focus_window_by_title(const char *title) {
  if (!title) {
    return false;
  }
  return Platform_FocusWindow(title);
}

SkWindowHandle sk_get_window_id(const char *title) {
  return Platform_GetWindowByTitle(title);
}

SkWindowHandle sk_get_window_by_root_atom(const char *atom) {
  return Platform_GetWindowByRootAtom(atom);
}

bool sk_window_set_string_property(SkWindowHandle window, const char *atom,
                                   const char *value) {
  return Platform_SetWindowStringProperty(window, atom, value);
}

bool sk_window_set_long_property(SkWindowHandle window, const char *atom,
                                 long value) {
  return Platform_SetWindowLongProperty(window, atom, value);
}

bool sk_window_send_message(SkWindowHandle window, const char *atom,
                            long data) {
  return Platform_SendWindowMessage(window, atom, data);
}

bool sk_close_active_window(void) { return Platform_CloseActiveWindow(); }

bool sk_dismiss_message(const char *name) {
  return sk_dismiss_message_platform(name);
}

bool sk_message_active(const char *name) {
  return Platform_MessageActive(name);
}

bool sk_toggle_fullscreen_active(void) {
  return Platform_ToggleFullscreenActive();
}

bool sk_move_active_window_to_monitor(bool right) {
  return Platform_MoveActiveWindowToMonitor(right);
}

bool sk_move_active_window_left(void) {
  return Platform_MoveActiveWindowLeft();
}

bool sk_move_active_window_right(void) {
  return Platform_MoveActiveWindowRight();
}

bool sk_focus_window_left(void) { return Platform_FocusWindowLeft(); }

bool sk_focus_window_right(void) { return Platform_FocusWindowRight(); }

bool sk_move_active_window_up(void) { return Platform_MoveActiveWindowUp(); }

bool sk_move_active_window_down(void) {
  return Platform_MoveActiveWindowDown();
}

bool sk_focus_window_up(void) { return Platform_FocusWindowUp(); }

bool sk_focus_window_down(void) { return Platform_FocusWindowDown(); }

bool sk_toggle_minimized_active(void) {
  return Platform_MinimizeActiveWindow();
}

bool sk_toggle_maximize_active(void) {
  return Platform_ToggleMaximizeActiveWindow();
}

bool sk_toggle_always_on_top_active(void) {
  return Platform_ToggleAlwaysOnTopActiveWindow();
}

bool sk_raise_active_window(void) { return Platform_RaiseActiveWindow(); }

bool sk_lower_active_window(void) { return Platform_LowerActiveWindow(); }

bool sk_center_active_window(void) { return Platform_CenterActiveWindow(); }

bool sk_snap_active_window(SnapPreset preset) {
  return Platform_SnapActiveWindow(preset);
}

long sk_get_active_window_pid(void) { return Platform_GetActiveWindowPid(); }

bool sk_switch_to_workspace(int index) {
  return Platform_SwitchToWorkspace(index);
}

bool sk_move_active_window_to_workspace(int index) {
  return Platform_MoveActiveWindowToWorkspace(index);
}

bool sk_set_active_window_opacity(int percent) {
  return Platform_SetActiveWindowOpacity(percent);
}

bool sk_close_windows_by_title(const char *needle) {
  return Platform_CloseWindowsByTitle(needle);
}

bool sk_close_windows_by_id(const char *needle) {
  return Platform_CloseWindowsById(needle);
}

bool sk_reload_config(void) { return config_reload(); }

const char *sk_get_theme(void) { return (Theme && Theme[0]) ? Theme : NULL; }

int sk_theme_count(void) { return (int)theme_count(); }

const char *sk_theme_name_at(int index) {
  if (index < 0)
    return NULL;
  const ThemeEntry *e = theme_entry_at((size_t)index);
  return e ? e->name : NULL;
}

bool sk_set_theme(const char *name) {
  if (!name || !name[0])
    return false;
  const ThemeEntry *e = theme_find(name);
  if (!e)
    return false;
  Theme = e->name;
  theme_apply();
  config_save();
  UI_RequestPopupRepaint();
  return true;
}

char *ski_popup(const PopupSpec *spec) {
  if (!spec)
    return NULL;
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  PromptSpec pspec = {0};
  pspec.placeholder = spec->placeholder;
  pspec.enter = popup_enter;
  pspec.password_mode = spec->password_mode;
  pspec.x = spec->x;
  pspec.y = spec->y;
  pspec.w = spec->w ? spec->w : 600;
  pspec.h = spec->h ? spec->h : 70;
  char *r = WindowManager_ShowPrompt(wm, &pspec);
  return r ? strdup(r) : NULL;
}

static __thread ListBoxInfo *sk_list_lb;
static __thread ListSpec sk_list_spec;
static __thread int sk_list_y0, sk_list_itemH, sk_list_visible, sk_list_cur,
    sk_list_h;
static __thread int sk_list_bx, sk_list_bw, sk_list_fw, sk_list_fh;

unsigned long sk_listbox_get_current_standard_color(void) {
  if (!sk_list_lb || sk_list_cur == 0)
    return 0;
  int idx = sk_list_lb->view_offset + sk_list_cur - 1;
  if (idx < 0 || idx >= sk_list_lb->item_count)
    return 0;
  unsigned long ncol, bcol, pcol;
  item_get_colors((GetItemColorsSpec){.idx = idx,
                                      .count = sk_list_lb->item_count,
                                      .num_color = &ncol,
                                      .body_color = &bcol,
                                      .plain_color = &pcol});
  return pcol;
}

bool ski_draw_text(const SkTextSpec *spec) {
  if (!spec) {
    return false;
  }
  const char *real_text = spec->text;
  char tmp[2] = {0};
  if ((uintptr_t)real_text < 256) {
    if (!real_text) {
      return false;
    }
    tmp[0] = (char)(uintptr_t)real_text;
    tmp[1] = '\0';
    real_text = tmp;
  }
  if (!real_text) {
    return false;
  }
  unsigned long col = spec->color;
  if (col == COLOR_AUTO) {
    unsigned long cur = sk_listbox_get_current_standard_color();
    col = cur ? cur : rgb_to_hex(COLOR_TEXT);
  } else {
    col = rgb_to_hex(col);
  }
  int draw_x = spec->x - (spec->no_hscroll ? 0 : listbox_get_row_h_scroll());
  int draw_y = spec->y;
  if (draw_state_get()->listbox_rows && draw_state_get()->win32_listbox_surface)
    draw_y += 3;
  const char *align = spec->align ? spec->align : "LEFT";
  FontHandle fh = spec->font;
  return Platform_DrawTextRaw((DrawTextRawSpec){.x = draw_x,
                                                .y = draw_y,
                                                .text = real_text,
                                                .color = col,
                                                .font = fh,
                                                .align = (char *)align,
                                                .pad = spec->pad,
                                                .fontsize = spec->fontsize,
                                                .angle = spec->angle});
}

typedef int (*SkWrapMeasureFn)(void *ctx, const char *s, int n);

typedef struct {
  unsigned long color;
  int fontsize;
} SkWrapCtx;

static void wrap_record(SkCharPos *out, int maxout, int *count, int x, int y) {
  int i = *count;
  (*count)++;
  if (out && i < maxout) {
    out[i].x = x;
    out[i].y = y;
  }
}

static int wrap_place_word(SkWrapMeasureFn measure, void *ctx, const char *text,
                           int ws, int we, int wx, int rowtop, SkCharPos *out,
                           int maxout, int *count) {

  int i = ws;
  while (i < we) {
    int j = i + utf8_lead_len((unsigned char)text[i]);
    if (j > we)
      j = we;
    int px = wx + measure(ctx, text + ws, i - ws);
    for (int m = i; m < j; m++)
      wrap_record(out, maxout, count, px, rowtop);
    i = j;
  }
  return measure(ctx, text + ws, we - ws);
}

static int wrap_layout(const char *text, int x0, int y0, int max_width,
                       int line_height, SkWrapMeasureFn measure, void *ctx,
                       SkCharPos *out, int maxout) {

  int count = 0;
  int len = (int)strlen(text);
  int avail = max_width;
  if (avail < 40)
    avail = 40;
  int space_w = measure(ctx, " ", 1);
  int row = 0;
  int p = 0;
  int lw = 0;
  bool has = false;
  while (p < len) {
    if (text[p] == '\n') {
      wrap_record(out, maxout, &count, x0, y0 + (row + 1) * line_height);
      row++;
      lw = 0;
      has = false;
      p++;
      continue;
    }
    int ws = p;
    while (ws < len && (text[ws] == ' ' || text[ws] == '\t'))
      ws++;
    for (int m = p; m < ws; m++)
      wrap_record(out, maxout, &count, x0 + (has ? lw : 0),
                  y0 + row * line_height);
    if (ws >= len)
      break;
    if (text[ws] == '\n') {
      p = ws;
      continue;
    }
    int we = ws;
    while (we < len && text[we] != ' ' && text[we] != '\t' && text[we] != '\n')
      we++;
    int ww = measure(ctx, text + ws, we - ws);
    int rowtop = y0 + row * line_height;
    if (ww > avail) {
      if (has) {
        row++;
        rowtop = y0 + row * line_height;
        lw = 0;
        has = false;
      }
      int c = ws;
      while (c < we) {
        int lo = 0, hi = we - c;
        while (lo < hi) {
          int mid = (lo + hi + 1) / 2;
          if (measure(ctx, text + c, mid) <= avail)
            lo = mid;
          else
            hi = mid - 1;
        }
        int n = lo;
        while (n > 0 && (((unsigned char)text[c + n] & 0xC0) == 0x80))
          n--;
        if (n <= 0) {
          n = utf8_lead_len((unsigned char)text[c]);
          if (n > we - c)
            n = we - c;
        }
        if (c + n >= we) {
          lw = wrap_place_word(measure, ctx, text, c, we, x0, rowtop, out,
                               maxout, &count);
          has = true;
          c = we;
          break;
        }
        wrap_place_word(measure, ctx, text, c, c + n, x0, rowtop, out, maxout,
                        &count);
        row++;
        rowtop = y0 + row * line_height;
        c += n;
      }
      p = we;
      continue;
    }
    int add = ww + (has ? space_w : 0);
    if (has && lw + add > avail) {
      row++;
      rowtop = y0 + row * line_height;
      lw = 0;
      has = false;
    }
    int wx = x0 + (has ? lw + space_w : 0);
    int pxw = wrap_place_word(measure, ctx, text, ws, we, wx, rowtop, out,
                              maxout, &count);
    lw = has ? lw + space_w + pxw : pxw;
    has = true;
    p = we;
  }
  return count;
}

static int wrap_measure_thunk(void *ctx, const char *s, int n) {

  SkWrapCtx *c = (SkWrapCtx *)ctx;
  return Platform_MeasureTextWidth(
      (MeasureTextWidthSpec){.text = s, .len = n, .fontsize = c->fontsize});
}

int ski_measure_text_wrapped(const SkMeasureSpec *spec) {
  if (!spec || !spec->text) {
    return 0;
  }
  int fontsize = spec->fontsize;
  if (fontsize <= 0)
    fontsize = FontSize;
  int line_height = spec->line_height;
  if (line_height <= 0)
    line_height = fontsize + 4;
  SkWrapCtx ctx;
  ctx.color = 0;
  ctx.fontsize = fontsize;
  int cap = (int)strlen(spec->text) + 1;
  return wrap_layout(spec->text, spec->x, spec->y, spec->max_width, line_height,
                     wrap_measure_thunk, &ctx, spec->out_positions, cap);
}

bool ski_place_editbox(const SkEditSpec *spec) {
  if (!spec) {
    return false;
  }
  SkaldContext_Get()->window.editbox_placed = true;
  return sk_place_editbox_platform(spec);
}

bool ski_draw_image(const SkImageSpec *spec) {
  if (!spec || !spec->path || !spec->path[0]) {
    return false;
  }
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  char asset[MAX_PATH];
  const char *path = spec->path;
  if ((path[0] == '/' || path[0] == '\\') &&
      feature_path_resolve_asset_path(__builtin_return_address(0), path, asset,
                                      sizeof(asset))) {
    path = asset;
  }
  return Platform_DrawImageAt(path, spec->x, spec->y, w, h);
}

bool ski_draw_rect(const DrawRect *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  bool transparent = spec->transparent;
  if (col == COLOR_AUTO)
    col = COLOR_TEXT;
  col = rgb_to_hex(col);
  return Platform_DrawRectRaw((DrawRectRawSpec){
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .color = col,
      .radius = spec->radius ? spec->radius : 0,
      .rounded = spec->rounded ? spec->rounded : RoundedCorners,
      .opaque = !transparent,
      .thickness = spec->thickness,
      .filled = spec->filled,
      .angle = spec->angle});
}

int sk_rand_num_between(int min, int max) {
  if (max < min) {
    int t = min;
    min = max;
    max = t;
  }
  unsigned long span = (unsigned long)((long)max - (long)min) + 1UL;
  if (span == 0UL)
    return min;
  unsigned long limit = (unsigned long)RAND_MAX + 1UL;
  unsigned long r = (unsigned long)rand();
  if (span > limit) {
    r = (r << 31) | (unsigned long)rand();
  }
  return min + (int)(r % span);
}

SkRect ski_text_bounds(const SkTextBoundsSpec *spec) {
  SkRect r = {.x = 0, .y = 0, .w = 0, .h = 0, .color = 0};
  if (!spec || !spec->text || !spec->text[0])
    return r;
  int fs = spec->fontsize > 0 ? spec->fontsize : FontSize;
  const char *a = spec->align ? spec->align : "CENTER";
  int w = Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = spec->text, .len = (int)strlen(spec->text), .fontsize = fs});
  int h = Platform_GetTextLineHeight(fs);
  int base = spec->pad > 0 ? spec->pad : spec->x;
  if (string_contains_ci(a, "LEFT"))
    r.x = base;
  else if (string_contains_ci(a, "RIGHT"))
    r.x = base - w;
  else
    r.x = base - w / 2;
  r.y = spec->y;
  if (draw_state_get()->listbox_rows && draw_state_get()->win32_listbox_surface)
    r.y += 3;
  r.w = w;
  r.h = h;
  return r;
}

int sk_get_text_width(const char *text, int fontsize) {
  if (!text)
    return 0;
  return Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = text, .len = (int)strlen(text), .fontsize = fontsize});
}

int sk_get_text_height(int fontsize) {
  return Platform_GetTextLineHeight(fontsize);
}

ColorRef sk_color_lerp(ColorRef a, ColorRef b, float t) {
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

void sk_set_draw_offset(int dx, int dy) { set_draw_offset(dx, dy); }

void sk_get_draw_offset(int *out_x, int *out_y) {
  get_draw_offset(out_x, out_y);
}
char *sk_http_get(const char *url) { return http_get(url); }

char *sk_http_get_limited(const char *url, size_t max_bytes, size_t *out_len) {
  return http_get_limited(url, max_bytes, out_len);
}

bool sk_http_download(const char *url, const char *save_path) {
  return http_download(url, save_path);
}

char *sk_http_post(const char *url, const char *content_type,
                   const char *body) {
  return http_post(url, content_type, body);
}

void sk_http_get_async(const char *url, SkHttpCallback cb, void *userdata) {
  http_get_async(url, cb, userdata);
  return;
}

bool ski_draw_line(const Line *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  if (col == COLOR_AUTO)
    col = COLOR_TEXT;
  else
    col = rgb_to_hex(col);
  return Platform_DrawLineRaw((DrawLineRawSpec){.x1 = spec->x1,
                                                .y1 = spec->y1,
                                                .x2 = spec->x2,
                                                .y2 = spec->y2,
                                                .color = col,
                                                .thickness = spec->thickness});
}

bool ski_draw_circle(const Circle *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  if (col == COLOR_AUTO)
    col = COLOR_TEXT;
  else
    col = rgb_to_hex(col);
  return Platform_DrawCircleRaw(
      (DrawCircleRawSpec){.cx = spec->x,
                          .cy = spec->y,
                          .radius = spec->radius,
                          .color = col,
                          .thickness = spec->thickness});
}

static bool listbox_begin_common(const ListSpec *spec) {
  if (!spec)
    return false;
  ListBoxInfo *lb = spec->lb;
  if (!lb) {
    return false;
  }
  int item_h = spec->item_height ? spec->item_height : 0;
  ListboxBackendInit bi;
  if (!Platform_ListboxBackendBegin(item_h, &bi))
    return false;
  item_h = item_h > 0 ? item_h : bi.item_h;
  PromptState *st = Platform_ListboxBackendPromptState(lb);
  ListboxLayout lay;
  listbox_compute_layout(spec, bi.list_top, item_h,
                         st ? st->width : PromptWidth,
                         st ? st->height : PromptHeight, lb->item_count, &lay);
  sk_list_spec = *spec;
  sk_list_lb = lb;
  sk_list_itemH = item_h;
  sk_list_y0 = lay.y0;
  sk_list_bx = lay.bx;
  sk_list_bw = lay.bw;
  sk_list_fw = lay.fw;
  sk_list_fh = lay.fh;
  sk_list_h = lay.list_h;
  sk_list_visible = lay.visible;
  sk_list_cur = 0;
  UI_LB_SetVisibleCount(lay.visible);
  listbox_apply_pending_scroll(lb, lay.visible);
  listbox_set_geometry(sk_list_y0, sk_list_itemH, lay.visible);
  listbox_hover_focus(lb, sk_list_y0, sk_list_itemH, lb->view_offset,
                      lay.visible, sk_list_bx, sk_list_bx + sk_list_bw);
  draw_state_get()->listbox_rows = true;
  return true;
}

static bool listbox_next_common(SkListItemCtx *out) {
  if (!out || !sk_list_lb)
    return false;
  if (sk_list_cur >= sk_list_visible)
    return false;
  int idx = sk_list_lb->view_offset + sk_list_cur;
  if (idx < 0 || idx >= sk_list_lb->item_count)
    return false;
  int y = sk_list_y0 + sk_list_cur * sk_list_itemH;
  if (sk_list_cur == 0)
    draw_prompt_separator(
        (DrawPromptSeparatorSpec){.y = sk_list_y0, .width = sk_list_fw});
  bool sel = (idx == sk_list_lb->selected_index);
  Platform_ListboxBackendDrawSelection(sk_list_bx, sk_list_bw, y, sk_list_itemH,
                                       sk_list_fw, sk_list_fh, sel);
  listbox_set_row_h_scroll(sel ? sk_list_lb->text_hscroll : 0);
  out->idx = idx;
  out->selected = sel;
  out->x = sk_list_spec.x;
  out->y = y;
  out->w = sk_list_spec.w;
  out->h = sk_list_itemH;
  out->text = sk_list_lb->items[idx];
  sk_list_cur++;
  return true;
}

void sk_listbox_end(void) {
  listbox_set_row_h_scroll(0);
  if (prompt_list_bottom_margin() > 0 && sk_list_y0 >= 0) {
    draw_prompt_separator((DrawPromptSeparatorSpec){.y = sk_list_y0 + sk_list_h,
                                                    .width = sk_list_fw});
  }
  Platform_ListboxBackendEnd();
  sk_list_lb = NULL;
  sk_list_visible = 0;
  sk_list_cur = 0;
  sk_list_y0 = -1;
  draw_state_get()->listbox_rows = false;
}

bool ski_listbox_begin(const SkListSpec *spec) {
  if (!spec)
    return false;
  return listbox_begin_common(spec);
}

bool sk_listbox_next(SkListItemCtx *out) {
  if (!out)
    return false;
  return listbox_next_common(out);
}

int sk_listbox_get_count(void) {
  return sk_list_lb ? sk_list_lb->item_count : 0;
}

bool ski_place_listbox(const SkListSpec *spec) {
  if (!spec) {
    return false;
  }
  if (!ski_listbox_begin(spec)) {
    return false;
  }
  int line_h = Platform_GetTextLineHeight(0);
  int pad_top = (sk_list_itemH - line_h) / 2;
  if (pad_top < 0)
    pad_top = 0;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx)) {
    sk_draw_text(.x = ctx.x + 10, .y = ctx.y + pad_top, .text = ctx.text,
                 .align = "LEFT", .pad = ctx.x + 10);
  }
  sk_listbox_end();
  return true;
}

bool sk_checkbox_is_checked(const char *name) {
  return checkbox_is_checked(name, PLACE_CHECKBOX);
}

bool sk_checkbox_is_hovered(const char *name) {
  return checkbox_is_hovered(name, PLACE_CHECKBOX);
}

bool sk_checkbox_is_clicked(const char *name) {
  return checkbox_is_clicked(name, PLACE_CHECKBOX);
}

bool sk_button_is_checked(const char *name) {
  return checkbox_is_checked(name, PLACE_BUTTON);
}

bool sk_button_is_hovered(const char *name) {
  return checkbox_is_hovered(name, PLACE_BUTTON);
}

bool sk_button_is_clicked(const char *name) {
  return checkbox_is_clicked(name, PLACE_BUTTON);
}

bool ski_place_checkbox(const SkCheckboxSpec *spec) {
  return checkbox_place(spec);
}

bool ski_place_slider(const SkSliderSpec *spec) { return slider_place(spec); }

bool sk_slider_is_hovered(const char *name) { return slider_is_hovered(name); }
bool sk_slider_is_clicked(const char *name) { return slider_is_clicked(name); }
bool sk_slider_is_dragging(const char *name) {
  return slider_is_dragging(name);
}
float sk_slider_value(const char *name) { return slider_value(name); }
float sk_slider_fraction(const char *name) { return slider_fraction(name); }
int sk_slider_percent(const char *name) { return slider_percent(name); }
bool sk_slider_set_value(const char *name, float value) {
  return slider_set_value(name, value);
}

SkWindowList sk_get_all_window_titles(void) {
  return Platform_GetWindowTitles();
}

SkWindowList sk_get_all_window_ids(void) { return Platform_GetWindowIds(); }

SkColor ski_get_pixel_color(int x, int y) {
  return Platform_GetPixelColor(x, y);
}
SkColor sk_get_pixel_color(SkPos pos) {
  return ski_get_pixel_color(pos.x, pos.y);
}
bool ski_color_matches(int x, int y, size_t r, size_t g, size_t b) {
  SkColor c = ski_get_pixel_color(x, y);
  return c.r == r && c.g == g && c.b == b;
}
bool sk_color_matches(SkPos pos, SkColor color) {
  return ski_color_matches(pos.x, pos.y, color.r, color.g, color.b);
}
bool ski_move_to_pos(int x, int y) { return Platform_MoveToPos(x, y); }
bool sk_move_to_pos(SkPos pos) { return ski_move_to_pos(pos.x, pos.y); }
bool ski_click_at(int x, int y) { return ski_move_to_pos(x, y) && sk_click(); }
bool sk_click_at(SkPos pos) { return ski_click_at(pos.x, pos.y); }
bool sk_left_click(void) { return Platform_ClickButton(1); }
bool sk_right_click(void) { return Platform_ClickButton(2); }
bool sk_emit_mouse_button(int button, bool down) {
  return Platform_MouseButtonDown(button, down);
}
bool sk_emit_mouse_button_at(SkPos pos, int button, bool down) {
  bool moved = sk_move_to_pos(pos);
  return moved && Platform_MouseButtonDown(button, down);
}
bool sk_click_button_at(SkPos pos, int button) {
  bool moved = sk_move_to_pos(pos);
  bool down = moved && Platform_MouseButtonDown(button, true);
  if (!down) {
    return false;
  }
  sk_sleep(0.02);
  return Platform_MouseButtonDown(button, false);
}
bool sk_mouse_scroll(int notches) { return Platform_MouseScroll(notches); }

static bool rel_to_abs(int rx, int ry, SkPos *out) {

  if (!out) {
    return false;
  }
  int vx = 0, vy = 0, vw = 0, vh = 0;
  if (!Platform_GetVirtualBounds(&vx, &vy, &vw, &vh)) {
    return false;
  }
  out->x = vx + rx;
  out->y = vy + ry;
  return true;
}

bool sk_move_to_pos_rel(int rx, int ry) {
  SkPos abs;
  if (!rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  return sk_move_to_pos(abs);
}

bool sk_click_at_rel(int rx, int ry) {
  SkPos abs;
  if (!rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  return sk_click_at(abs);
}

bool sk_emit_mouse_button_at_rel(int rx, int ry, int button, bool down) {
  SkPos abs;
  if (!rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  return sk_emit_mouse_button_at(abs, button, down);
}

bool sk_click_button_at_rel(int rx, int ry, int button) {
  SkPos abs;
  if (!rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  return sk_click_button_at(abs, button);
}

void sk_emit_key(int vk, bool down) { Platform_EmitKey(vk, down ? 1 : 0); }

bool sk_replay_hotkey(int vk, int mods) { return hotkey_process(vk, mods); }
static bool type_text_common(const char *s) {
  if (!s)
    return false;
  char *old = Platform_GetClipboardEntry();
  // yes this is literally just ctrl c ctrl v
  Platform_CopyToClipboard(s);
  Platform_EmitKey(VK_LCONTROL, 1);
  Platform_EmitKey('V', 1);
  Platform_EmitKey('V', 0);
  Platform_EmitKey(VK_LCONTROL, 0);
  WindowManager *wm = WindowManager_Get();
  // we have to wait ~200ms for the WM to process the paste event before
  // restoring the clipboard
  for (int i = 0; i < 40; i++) {
    if (wm)
      WindowManager_ProcessEvents(wm);
    sk_sleep_platform(0.005);
  }
  if (old) {
    Platform_CopyToClipboard(old);
    free(old);
  }
  return true;
}

bool sk_type_text(const char *s) { return type_text_common(s); }

void sk_sleep(double seconds) { sk_sleep_platform(seconds); }

#define SKI_LIB_TRACK_MAX 32

typedef struct {
  SkLibrary lib;
  char canon[MAX_PATH];
} SkiLibTrack;

static SkiLibTrack ski_lib_track[SKI_LIB_TRACK_MAX];
static size_t ski_lib_track_count = 0;

static bool sk_lib_canonical_path(const char *path, char *out, size_t cap) {
  if (!path || !out || cap == 0)
    return false;
#if defined(_WIN32)
  return _fullpath(out, path, cap) != NULL;
#else
  char *resolved = realpath(path, NULL);
  if (!resolved)
    return false;
  snprintf(out, cap, "%s", resolved);
  free(resolved);
  return true;
#endif
}

static bool sk_lib_path_equal(const char *a, const char *b) {
#if defined(_WIN32)
  return _stricmp(a, b) == 0;
#else
  return strcmp(a, b) == 0;
#endif
}

static void ski_lib_track_add(SkLibrary lib, const char *canon) {
  if (ski_lib_track_count >= SKI_LIB_TRACK_MAX) {
    log_error("sk_library_open: tracking table full, not tracking '%s'", canon);
    return;
  }
  SkiLibTrack *e = &ski_lib_track[ski_lib_track_count++];
  e->lib = lib;
  snprintf(e->canon, sizeof(e->canon), "%s", canon);
}

static void ski_lib_track_remove(size_t idx) {
  ski_lib_track[idx] = ski_lib_track[ski_lib_track_count - 1];
  ski_lib_track_count--;
}

SkLibrary sk_library_open(const char *path) {
  SkLibrary lib = (SkLibrary)Platform_LoadLibrary(path);
  if (!lib)
    return NULL;
  char canon[MAX_PATH];
  if (sk_lib_canonical_path(path, canon, sizeof(canon)))
    ski_lib_track_add(lib, canon);
  return lib;
}

void *sk_library_symbol(SkLibrary lib, const char *name) {
  return Platform_GetSymbol((LibHandle)lib, name);
}

void sk_library_close(SkLibrary lib) {
  if (!lib)
    return;
  char caller[MAX_PATH];
  owner_resolve_for_func((void *)__builtin_return_address(0), caller,
                         sizeof(caller));
  char canon_caller[MAX_PATH];
  bool have_caller = caller[0] && sk_lib_canonical_path(caller, canon_caller,
                                                        sizeof(canon_caller));
  for (size_t i = 0; i < ski_lib_track_count; i++) {
    if (ski_lib_track[i].lib == lib && have_caller &&
        sk_lib_path_equal(ski_lib_track[i].canon, canon_caller)) {
      log_error(
          "sk_library_close: refusing to unload the caller's own library");
      return;
    }
  }
  for (size_t i = 0; i < ski_lib_track_count; i++) {
    if (ski_lib_track[i].lib == lib) {
      ski_lib_track_remove(i);
      break;
    }
  }
  Platform_UnloadLibrary((LibHandle)lib);
}

const char *sk_library_error(void) { return Platform_GetLoadError(); }

typedef struct {
  SkThreadFunc fn;
  void *arg;
  char owner[512];
} SkThreadCtx;

static void *thread_trampoline(void *p) {
  SkThreadCtx *ctx = (SkThreadCtx *)p;
  SkThreadFunc fn = ctx->fn;
  void *arg = ctx->arg;
  void *ret = fn ? fn(arg) : NULL;
  thread_registry_finish(ctx);
  free(ctx);
  return ret;
}

static SkThread thread_create_ex(SkThreadFunc fn, void *arg, void *owner_src) {
  if (!fn)
    return NULL;
  SkThreadCtx *ctx = malloc(sizeof(*ctx));
  if (!ctx)
    return NULL;
  ctx->fn = fn;
  ctx->arg = arg;
  ctx->owner[0] = '\0';
  Platform_GetModuleOwnerPath(owner_src ? owner_src : (void *)(uintptr_t)fn,
                              ctx->owner, sizeof(ctx->owner));
  thread_registry_add(ctx, ctx->owner);
  void *thread = Platform_ThreadCreate(thread_trampoline, ctx);
  if (!thread) {
    thread_registry_finish(ctx);
    free(ctx);
    return NULL;
  }
  return (SkThread)thread;
}

SkThread sk_thread_create(SkThreadFunc fn, void *arg) {
  return thread_create_ex(fn, arg, NULL);
}

void sk_thread_join(SkThread thread) {
  if (!thread) {
    return;
  }
  Platform_ThreadJoin((void *)thread);
}

SkMutex sk_mutex_create(void) { return (SkMutex)Platform_MutexCreate(); }

void sk_mutex_destroy(SkMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexDestroy((void *)mutex);
}

void sk_mutex_lock(SkMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexLock((void *)mutex);
}

void sk_mutex_unlock(SkMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexUnlock((void *)mutex);
}

void sk_send_hotkey(const char *hotkey_string) {
  if (!hotkey_string) {
    return;
  }
  remap_send_combo(hotkey_string);
}

char *sk_hotkey_to_string(SkKey key) {
  char tmp[256];
  if (!hotkey_string_format(key, tmp, (int)sizeof(tmp)))
    return NULL;
  return strdup(tmp);
}

SkKey sk_hotkey_from_string(const char *str) {
  SkKey key = {0};
  hotkey_string_parse(str, &key);
  return key;
}

bool sk_is_key_down(int vk) { return input_is_key_down(vk); }
bool sk_is_key_pressed(int vk) { return input_was_key_pressed(vk); }
bool sk_is_key_released(int vk) { return input_was_key_released(vk); }
bool sk_ctrl_pressed(void) { return key_is_ctrl_pressed(); }
bool sk_shift_pressed(void) { return key_is_shift_pressed(); }
bool sk_alt_pressed(void) { return key_is_alt_pressed(); }
bool sk_super_pressed(void) { return key_is_super_pressed(); }
void sk_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud) {
  input_set_key_callback(cb, ud);
}
int sk_get_key_pressed(void) { return input_get_pressed_key(); }
char sk_get_char_pressed(void) { return input_get_pressed_char(); }

char *sk_vk_to_name(int vk) {
  char tmp[64];
  if (!input_vk_to_name(vk, tmp, sizeof(tmp)))
    return NULL;
  return strdup(tmp);
}

bool sk_poll_key(SkKeyEvent *out) {
  if (!out)
    return false;
  PlatformKeyEvent ev;
  if (!global_key_pop(&ev))
    return false;
  out->vk = ev.vk;
  out->scancode = ev.scancode;
  out->mods = ev.mods;
  out->down = ev.down;
  out->repeat = ev.repeat;
  out->t_us = ev.t_us;
  return true;
}

bool sk_poll_mouse(SkMouseEvent *out) {
  if (!out)
    return false;
  PlatformMouseEvent ev;
  if (!global_mouse_pop(&ev))
    return false;
  out->button = ev.button;
  out->x = ev.x;
  out->y = ev.y;
  out->mods = ev.mods;
  out->down = ev.down;
  out->repeat = ev.repeat;
  out->t_us = ev.t_us;
  return true;
}

int sk_mouse_queue_len(void) { return global_mouse_pending(); }

unsigned long sk_mouse_queue_dropped(void) { return global_mouse_dropped(); }

void sk_clear_mouse_queue(void) { global_mouse_clear(); }

SkPos sk_get_cursor_pos(void) {
  int x, y;
  Platform_GetMousePos(&x, &y);
  return (SkPos){x, y};
}

bool sk_mouse_pressed(int button) { return input_mouse_state_down(button); }

int sk_key_queue_len(void) { return global_key_pending(); }

unsigned long sk_key_queue_dropped(void) { return global_key_dropped(); }

void sk_clear_key_queue(void) { global_key_clear(); }

bool sk_is_flag_prefix(char c) { return c == '-' || c == '/'; }

char *sk_extract_flag_name(const char *p) {
  size_t n = 0;
  while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '=')
    n++;
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, p, n);
  out[n] = '\0';
  return out;
}

char *sk_extract_quoted(char **pp) {
  char *p = *pp;
  char q = *p++;
  char *s = p;
  while (*p && *p != q)
    p++;
  size_t len = (size_t)(p - s);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  if (*p == q)
    p++;
  *pp = p;
  return out;
}

char *sk_extract_value(char **pp) {
  char *p = *pp;
  if (*p == '"' || *p == '\'')
    return sk_extract_quoted(pp);
  char *s = p;
  while (*p && !isspace((unsigned char)*p))
    p++;
  size_t len = (size_t)(p - s);
  if (len == 0)
    return NULL;
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  *pp = p;
  return out;
}

bool sk_next_flag(char *p) {
  while (*p && isspace((unsigned char)*p))
    p++;
  return *p == '-' || *p == '/';
}

SkFlagList sk_parse_flags(const char *input) {
  SkFlagList out = {0};
  if (!input || !*input)
    return out;
  char *copy = strdup(input);
  if (!copy)
    return out;
  size_t cap = 8;
  SkFlag *arr = (SkFlag *)calloc(cap, sizeof(SkFlag));
  if (!arr) {
    free(copy);
    {
      return out;
    }
  }
  char *p = copy;
  while (*p) {
    while (*p && isspace((unsigned char)*p))
      p++;
    if (!*p)
      break;
    if (!sk_is_flag_prefix(*p)) {
      while (*p && !isspace((unsigned char)*p))
        p++;
      continue;
    }
    p++;
    if (*p == '-')
      p++;
    char *name = sk_extract_flag_name(p);
    if (!name)
      break;
    p += strlen(name);
    char *valbuf = NULL;
    bool has_val = false;
    if (*p == '=') {
      p++;
      valbuf = sk_extract_value(&p);
      has_val = valbuf != NULL;
    } else {
      char *save = p;
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p && !sk_is_flag_prefix(*p)) {
        valbuf = sk_extract_value(&p);
        has_val = valbuf != NULL;
        if (has_val && sk_next_flag(p)) {
          has_val = false;
          free(valbuf);
          valbuf = NULL;
          p = save;
          while (*p && isspace((unsigned char)*p))
            p++;
        }
      }
    }
    if (out.count >= cap) {
      cap *= 2;
      SkFlag *narr = (SkFlag *)realloc(arr, cap * sizeof(SkFlag));
      if (!narr)
        break;
      arr = narr;
    }
    SkFlag *f = &arr[out.count++];
    memset(f, 0, sizeof(*f));
    snprintf(f->name, sizeof(f->name), "%s", name);
    free(name);
    if (has_val) {
      snprintf(f->value, sizeof(f->value), "%s", valbuf);
      f->has_value = true;
      free(valbuf);
    }
  }
  free(copy);
  out.flags = arr;
  if (out.count == 0) {
    free(arr);
    out.flags = NULL;
  }
  return out;
}

void sk_free_flags(SkFlagList *list) {
  if (!list) {
    return;
  }
  free(list->flags);
  list->flags = NULL;
  list->count = 0;
}

void sk_run_after_time(double seconds, void (*callback)(void)) {
  if (!callback || seconds < 0)
    return;
  SkRunAfterPayload *payload =
      (SkRunAfterPayload *)malloc(sizeof(SkRunAfterPayload));
  if (!payload)
    return;
  payload->seconds = seconds;
  payload->callback = callback;
  payload->owner[0] = '\0';
  owner_resolve_for_func((void *)callback, payload->owner,
                         sizeof(payload->owner));
  thread_registry_add(payload, payload->owner);
  if (!sk_run_after_time_arm_platform(payload)) {
    thread_registry_finish(payload);
    free(payload);
  }
}

SkTimer sk_start_timer(void) { return Platform_StartTimer(); }

double ski_end_timer(uint64_t ticks) {
  return Platform_EndTimer((SkTimer){.ticks = ticks});
}

double sk_end_timer(SkTimer t) { return ski_end_timer(t.ticks); }

char *sk_get_active_window_title(void) {
  char *title = Platform_GetActiveWindowTitle();
  if (!title || !title[0]) {
    return NULL;
  }
  return strdup(title);
}

char *sk_get_active_window_id(void) {
  char *id = Platform_GetActiveWindowId();
  if (!id || !id[0]) {
    return NULL;
  }
  return strdup(id);
}

bool sk_is_window_open(const char *title) {
  SkWindowList l = Platform_GetWindowTitles();
  bool found = false;
  for (int i = 0; i < l.count; i++) {
    if (string_contains_ci(l.windows[i], title)) {
      found = true;
      break;
    }
  }
  sk_free_window_list(&l);
  return found;
}

char *sk_get_working_dir(void) { return Platform_GetWorkingDir(); }

char *sk_resolve_data_path(const char *name) {
  if (!name || !name[0])
    return NULL;
  char full[MAX_PATH];
  if (!sk_resolve_caller_path_silent(__builtin_return_address(0), name, full,
                                     sizeof(full)))
    return NULL;
  return strdup(full);
}

char *sk_resolve_feature_path(const char *name) {
  char buf[340];
  int len = snprintf(buf, sizeof(buf), "%s/%s", FeaturePath, name);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return NULL;
  }

  return strdup(buf);
}

const char *sk_resolve_user_path(void) { return Platform_GetRealHome(); }

void sk_hashmap_init(SkHashMap **map) {
  if (!map) {
    return;
  }
  sh_new_strdup(*map);
}

void sk_hashmap_free(SkHashMap **map) {
  if (!map) {
    return;
  }
  shfree(*map);
}

void sk_hashmap_put(SkHashMap **map, const char *key, long value) {
  if (!map || !key) {
    return;
  }
  shput(*map, (char *)key, value);
}

void sk_hashmap_add(SkHashMap **map, const char *key, long delta) {
  if (!map || !key) {
    return;
  }
  ptrdiff_t idx = shgeti(*map, (char *)key);
  if (idx >= 0)
    (*map)[idx].value += delta;
  else
    shput(*map, (char *)key, delta);
}

long sk_hashmap_get(SkHashMap *map, const char *key, long def) {
  if (!map || !key)
    return def;
  ptrdiff_t idx = shgeti(map, (char *)key);
  if (idx < 0)
    return def;
  return map[idx].value;
}

bool sk_hashmap_has(SkHashMap *map, const char *key) {
  if (!map || !key)
    return false;
  return shgeti(map, (char *)key) >= 0;
}

bool sk_hashmap_del(SkHashMap **map, const char *key) {
  if (!map || !*map || !key)
    return false;
  return shdel(*map, (char *)key) != 0;
}

size_t sk_hashmap_len(SkHashMap *map) {
  if (!map)
    return 0;
  return shlenu(map);
}

const SkHashMap *sk_hashmap_at(SkHashMap *map, size_t index) {

  if (!map || index >= shlenu(map))
    return NULL;
  return &map[index];
}

SkTimer sk_profile_start(void) { return sk_start_timer(); }

void ski_profile_end(uint64_t ticks) {
  double ms = ski_end_timer(ticks);
  if (ms < 0.0)
    ms = 0.0;

  if (ms >= 1000.0)
    log_debug("[profile] %.3f s", ms / 1000.0);
  else if (ms >= 1.0)
    log_debug("[profile] %.3f ms", ms);
  else
    log_debug("[profile] %.3f µs", ms * 1000.0);
}

void sk_profile_end(SkTimer timer) { ski_profile_end(timer.ticks); }

typedef struct {
  char name[64];
  SkLibrary handle;
} LibraryEntry;

static LibraryEntry library_registry[32];
static int registry_count = 0;

void sk_library_register(const char *name, SkLibrary handle) {
  if (!name || !handle)
    return;
  for (int i = 0; i < registry_count; ++i) {
    if (strcmp(library_registry[i].name, name) == 0) {
      library_registry[i].handle = handle;
      return;
    }
  }
  if (registry_count < 32) {
    snprintf(library_registry[registry_count].name,
             sizeof(library_registry[registry_count].name), "%s", name);
    library_registry[registry_count].handle = handle;
    registry_count++;
  }
}

SkLibrary sk_library_get(const char *name) {
  if (!name)
    return NULL;
  for (int i = 0; i < registry_count; ++i) {
    if (strcmp(library_registry[i].name, name) == 0)
      return library_registry[i].handle;
  }
  return NULL;
}

SkWidget *sk_widget_create(const SkWidgetSpec *spec) {
  if (!spec)
    return NULL;
  SkWidgetSpec copy = *spec;
  if (copy.monitor_name && copy.monitor_name[0]) {
    int n = sk_monitor_count();
    int found = -1;
    for (int i = 0; i < n; i++) {
      SkMonitorInfo info;
      if (!sk_monitor_at(i, &info))
        continue;
      if (strcmp(info.name, copy.monitor_name) == 0) {
        found = i;
        break;
      }
    }
    if (found >= 0)
      copy.monitor = found;
    else
      log_warn("sk_widget: monitor '%s' not found, using primary",
               copy.monitor_name);
  }
  return widget_create(&copy);
}

bool sk_widget_valid(SkWidget *widget) { return widget_valid(widget); }

SkPos sk_get_widget_pos(const char *name) { return Widget_GetPos(name); }

void sk_widget_show(SkWidget *widget) { widget_show(widget); }

void sk_widget_hide(SkWidget *widget) { widget_hide(widget); }

void sk_widget_destroy(SkWidget *widget) { widget_destroy(widget); }

void sk_widget_set_render(SkWidget *widget, SkWidgetRenderFn render) {
  widget_set_render(widget, render);
}

char *sk_format_bytes(unsigned long long bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
  const int count = sizeof(units) / sizeof(units[0]);

  double value = (double)bytes;
  int i = 0;

  while (value >= 1024.0 && i < count - 1) {
    value /= 1024.0;
    i++;
  }

  char buf[64];
  if (i == 0) {
    snprintf(buf, sizeof(buf), "%llu %s", bytes, units[i]);
  } else {
    snprintf(buf, sizeof(buf), "%.1f %s", value, units[i]);
  }
  return strdup(buf);
}
SkNetStatus sk_net_status(void) { return sk_net_status_platform(); }

unsigned long long sk_net_rx_bytes(void) { return sk_net_rx_bytes_platform(); }

int sk_monitor_count(void) { return monitor_count(); }

bool sk_monitor_at(int index, SkMonitorInfo *out) {
  if (!out)
    return false;
  MonitorInfo info;
  if (!monitor_info(index, &info))
    return false;
  memset(out, 0, sizeof(*out));
  out->index = info.index;
  out->x = info.x;
  out->y = info.y;
  out->w = info.w;
  out->h = info.h;
  out->is_primary = info.is_primary;
  snprintf(out->name, sizeof(out->name), "%s", info.name);
  return true;
}

unsigned long long sk_net_rx_bytes_over(double seconds) {

  if (seconds <= 0)
    return 0;

  unsigned long long before = sk_net_rx_bytes_platform();
  if (before == 0)
    return 0;

  sk_sleep(seconds);

  unsigned long long after = sk_net_rx_bytes_platform();
  if (after < before)
    return 0;
  return after - before;
}

SkRect sk_get_monitor_geometry(void) {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return (DrawRect){.x = x, .y = y, .w = w, .h = h, .color = 0};
}

int sk_get_screen_width(void) {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return w;
}

int sk_get_screen_height(void) {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return h;
}

typedef struct {
  SkCcEntrySpec pub;
  char display_name[1024];
  char internal_name[1024];
  char icon[1024];
  char type[128];
  char owner[OWNER_PATH_MAX];
} SkCcEntry;

#define SETTINGS_MAX_ENTRIES 128

typedef struct {
  SettingsEntrySpec pub;
  char name[256];
  char icon[1024];
  char owner[OWNER_PATH_MAX];
} SkSettingsEntry;

static int settings_entry_count = 0;
static SkSettingsEntry settings_entries[SETTINGS_MAX_ENTRIES];

static int cc_entry_count = 0;
static SkCcEntry cc_entries[4096];

#define SK_CC_DEP_OWNERS 256
static char cc_dep_owners[SK_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int cc_dep_owner_count = 0;

static bool cc_dep_seen(const char *owner) {
  for (int i = 0; i < cc_dep_owner_count; i++)
    if (strcmp(cc_dep_owners[i], owner) == 0)
      return true;
  return false;
}

static void cc_dep_mark(const char *owner) {
  if (cc_dep_owner_count >= SK_CC_DEP_OWNERS)
    return;
  snprintf(cc_dep_owners[cc_dep_owner_count++], OWNER_PATH_MAX, "%s", owner);
}

static const char *cc_dup(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src ? src : "");
  return dst;
}

bool ski_add_to_cc(const SkCcEntrySpec *spec) {
  if (!spec)
    return false;
  int slot = -1;
  for (int i = 0; i < cc_entry_count; i++) {
    if (spec->display_name && cc_entries[i].display_name[0] &&
        strcmp(cc_entries[i].display_name, spec->display_name) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    if (cc_entry_count >= 4096)
      return false;
    slot = cc_entry_count++;
  }
  SkCcEntry *e = &cc_entries[slot];
  memset(e, 0, sizeof(*e));
  e->pub.on_enter = spec->on_enter;
  e->pub.on_ctrl_enter = spec->on_ctrl_enter;
  e->pub.on_shift_enter = spec->on_shift_enter;
  e->pub.on_ctrl_shift_enter = spec->on_ctrl_shift_enter;
  e->pub.on_ctrl_alt_enter = spec->on_ctrl_alt_enter;
  e->pub.on_shift_alt_enter = spec->on_shift_alt_enter;
  e->pub.on_tab = spec->on_tab;
  e->pub.on_action = spec->on_action;
  e->pub.display_name =
      cc_dup(e->display_name, sizeof(e->display_name), spec->display_name);
  char icon_asset[MAX_PATH];
  if (spec->icon && (spec->icon[0] == '/' || spec->icon[0] == '\\') &&
      feature_path_resolve_asset_path(__builtin_return_address(0), spec->icon,
                                      icon_asset, sizeof(icon_asset))) {
    e->pub.icon = cc_dup(e->icon, sizeof(e->icon), icon_asset);
  } else {
    e->pub.icon = cc_dup(e->icon, sizeof(e->icon), spec->icon);
  }
  e->pub.type = cc_dup(e->type, sizeof(e->type), spec->type);
  if (spec->internal_name) {
    e->pub.internal_name =
        cc_dup(e->internal_name, sizeof(e->internal_name), spec->internal_name);
  } else {
    e->pub.internal_name =
        cc_dup(e->internal_name, sizeof(e->internal_name), spec->display_name);
  }
  void *anchor = (void *)(uintptr_t)spec->on_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_shift_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_shift_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_alt_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_shift_alt_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_tab;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_action;
  owner_resolve_for_func(anchor, e->owner, sizeof(e->owner));
  if (e->owner[0] && !cc_dep_seen(e->owner)) {
    cc_dep_mark(e->owner);
    cc_add_dep("control_center.c", e->owner);
  }
  if (spec->type && strcmp(spec->type, "Feature") == 0)
    feature_list_add(spec->display_name, spec->on_action, spec->icon, e->owner);
  return true;
}

static void st_dup(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src ? src : "");
}

bool sk_settings_entry_add(const SettingsEntrySpec *spec) {
  if (!spec || !spec->name || !spec->name[0])
    return false;
  int slot = -1;
  for (int i = 0; i < settings_entry_count; i++) {
    if (settings_entries[i].name[0] &&
        strcmp(settings_entries[i].name, spec->name) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    if (settings_entry_count >= SETTINGS_MAX_ENTRIES)
      return false;
    slot = settings_entry_count++;
  }
  SkSettingsEntry *e = &settings_entries[slot];
  memset(e, 0, sizeof(*e));
  st_dup(e->name, sizeof(e->name), spec->name);
  char icon_asset[MAX_PATH];
  if (spec->icon && (spec->icon[0] == '/' || spec->icon[0] == '\\') &&
      feature_path_resolve_asset_path(__builtin_return_address(0), spec->icon,
                                      icon_asset, sizeof(icon_asset))) {
    st_dup(e->icon, sizeof(e->icon), icon_asset);
  } else {
    st_dup(e->icon, sizeof(e->icon), spec->icon);
  }
  e->pub.name = e->name;
  e->pub.icon = e->icon;
  e->pub.render = spec->render;
  e->pub.ud = spec->ud;
  if (spec->render)
    owner_resolve_for_func((void *)(uintptr_t)spec->render, e->owner,
                           sizeof(e->owner));
  return true;
}

int sk_settings_entry_count(void) { return settings_entry_count; }

const SettingsEntrySpec *sk_settings_entry_at(int index) {
  if (index < 0 || index >= settings_entry_count)
    return NULL;
  return &settings_entries[index].pub;
}

void settings_registry_purge_owner(const char *path_a, const char *path_b) {
  if (!path_a && !path_b)
    return;
  int out = 0;
  for (int i = 0; i < settings_entry_count; i++) {
    if (owner_module_matches(settings_entries[i].owner, path_a, path_b))
      continue;
    if (out != i) {
      settings_entries[out] = settings_entries[i];
      settings_entries[out].pub.name = settings_entries[out].name;
      settings_entries[out].pub.icon = settings_entries[out].icon;
    }
    out++;
  }
  for (int i = out; i < settings_entry_count; i++)
    memset(&settings_entries[i], 0, sizeof(settings_entries[i]));
  settings_entry_count = out;
}

int sk_cc_entry_count(void) { return cc_entry_count; }

const SkCcEntrySpec *sk_cc_entry_at(int index) {
  if (index < 0 || index >= cc_entry_count)
    return NULL;
  return &cc_entries[index].pub;
}

void cc_registry_purge_owner(const char *path_a, const char *path_b) {
  if (!path_a && !path_b)
    return;
  int out = 0;
  for (int i = 0; i < cc_entry_count; i++) {
    if (owner_module_matches(cc_entries[i].owner, path_a, path_b))
      continue;
    if (out != i) {
      cc_entries[out] = cc_entries[i];
      cc_entries[out].pub.display_name = cc_entries[out].display_name;
      cc_entries[out].pub.internal_name = cc_entries[out].internal_name;
      cc_entries[out].pub.icon = cc_entries[out].icon;
      cc_entries[out].pub.type = cc_entries[out].type;
    }
    out++;
  }
  for (int i = out; i < cc_entry_count; i++)
    memset(&cc_entries[i], 0, sizeof(cc_entries[i]));
  cc_entry_count = out;
}

#define SK_MAX_ALIASES 1024
static char alias_alias[SK_MAX_ALIASES][128];
static char alias_target[SK_MAX_ALIASES][256];
static int alias_count = 0;

bool ski_add_alias(const SkAliasSpec *spec) {
  if (!spec || !spec->alias || !spec->target)
    return false;
  if (alias_count >= SK_MAX_ALIASES)
    return false;
  for (int i = 0; i < alias_count; i++) {
    if (strcmp(alias_target[i], spec->target) == 0) {
      snprintf(alias_alias[i], sizeof(alias_alias[i]), "%s", spec->alias);
      return true;
    }
  }
  snprintf(alias_alias[alias_count], sizeof(alias_alias[alias_count]), "%s",
           spec->alias);
  snprintf(alias_target[alias_count], sizeof(alias_target[alias_count]), "%s",
           spec->target);
  alias_count++;
  return true;
}

bool ski_remove_alias(const char *alias) {
  if (!alias)
    return false;
  int n = 0;
  bool found = false;
  for (int i = 0; i < alias_count; i++) {
    if (strcmp(alias_alias[i], alias) == 0) {
      found = true;
      continue;
    }
    if (n != i) {
      memmove(alias_alias[n], alias_alias[i], sizeof(alias_alias[n]));
      memmove(alias_target[n], alias_target[i], sizeof(alias_target[n]));
    }
    n++;
  }
  alias_count = n;
  return found;
}

const char *sk_alias_for(const char *target) {
  if (!target)
    return NULL;
  for (int i = 0; i < alias_count; i++) {
    if (strcmp(alias_target[i], target) == 0)
      return alias_alias[i];
  }
  return NULL;
}

void ski_window(const SkPromptSpec *spec) {
  WindowManager_ShowPrompt(WindowManager_Get(), spec);
}

const char *sk_get_user_name(void) { return Platform_GetRealUser(); }

bool sk_is_root(void) { return Platform_IsRunningAsRoot(); }

void sk_reload_feature(const char *name) { feature_loader_reload(name); }

int sk_get_pid(void) { return sk_get_pid_platform(); }

int sk_reload_all_features(void) {
  return feature_loader_build_stale_sources(true);
}

bool ski_hotkey_down(int vk_code, int mods) {
  if (!sk_is_key_down(vk_code))
    return false;
  if ((mods & SK_CTRL) && !sk_ctrl_pressed())
    return false;
  if ((mods & SK_SHIFT) && !sk_shift_pressed())
    return false;
  if ((mods & SK_ALT) && !sk_alt_pressed())
    return false;
  if ((mods & SK_SUPER) && !sk_super_pressed())
    return false;
  return true;
}

bool sk_hotkey_down(SkKey key) {
  return ski_hotkey_down(key.vk_code, key.mods);
}

void sk_register_key_interceptor(SkKeyInterceptorFn fn) {
  key_register_interceptor((KeyInterceptorFn)fn);
}

typedef struct {
  double seconds;
  void (*callback)(void);
  int stop;
} SkRunEveryArgs;

typedef struct SkRunEveryEntry {
  SkRunEveryArgs *args;
  void *thread;
  char owner[OWNER_PATH_MAX];
  struct SkRunEveryEntry *next;
} SkRunEveryEntry;

static SkRunEveryEntry *run_every_head = NULL;
static void *run_every_mtx = NULL;

static void run_every_lock(void) {
  if (!run_every_mtx)
    run_every_mtx = Platform_MutexCreate();
  if (run_every_mtx)
    Platform_MutexLock(run_every_mtx);
}

static void run_every_unlock(void) {
  if (run_every_mtx)
    Platform_MutexUnlock(run_every_mtx);
}

static void *run_every_thread(void *arg) {
  SkRunEveryArgs *args = (SkRunEveryArgs *)arg;
  for (;;) {
    if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
      return NULL;
    double remaining = args->seconds;
    while (remaining > 0.0) {
      if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
        return NULL;
      double step = remaining >= 0.05 ? 0.05 : remaining;
      sk_sleep(step);
      remaining -= step;
    }
    if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
      return NULL;
    args->callback();
  }
}

void sk_run_every(double seconds, void (*callback)(void)) {
  if (!callback)
    return;
  if (!(seconds > 0.0))
    seconds = 0.01;
  SkRunEveryArgs *args = malloc(sizeof(SkRunEveryArgs));
  if (!args) {
    log_error("sk_run_every: malloc failed");
    return;
  }
  args->seconds = seconds;
  args->callback = callback;
  args->stop = 0;
  SkRunEveryEntry *entry = malloc(sizeof(SkRunEveryEntry));
  if (!entry) {
    log_error("sk_run_every: malloc failed");
    free(args);
    return;
  }
  entry->args = args;
  entry->owner[0] = '\0';
  entry->next = NULL;
  Platform_GetModuleOwnerPath((void *)(uintptr_t)callback, entry->owner,
                              sizeof(entry->owner));
  entry->owner[sizeof(entry->owner) - 1] = '\0';
  entry->thread =
      thread_create_ex(run_every_thread, args, (void *)(uintptr_t)callback);
  if (!entry->thread) {
    log_error("sk_run_every: thread creation failed");
    free(entry);
    free(args);
    return;
  }
  run_every_lock();
  entry->next = run_every_head;
  run_every_head = entry;
  run_every_unlock();
}

void run_every_purge_owner(const char *path_a, const char *path_b) {
  SkRunEveryEntry *mine = NULL;
  run_every_lock();
  SkRunEveryEntry **pp = &run_every_head;
  while (*pp) {
    SkRunEveryEntry *e = *pp;
    if (owner_module_matches(e->owner, path_a, path_b)) {
      *pp = e->next;
      e->next = mine;
      mine = e;
    } else {
      pp = &e->next;
    }
  }
  run_every_unlock();
  while (mine) {
    SkRunEveryEntry *e = mine;
    mine = e->next;
    __atomic_store_n(&e->args->stop, 1, __ATOMIC_RELAXED);
    if (e->thread)
      Platform_ThreadJoin(e->thread);
    free(e->args);
    free(e);
  }
}

SkMetrics *sk_get_metrics(void) { return metrics_get(); }

void *sk_get_active_prompt(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (top && top->user_data && top->type == POPUP_PROMPT) {
    return top->user_data;
  }
  return NULL;
}

void sk_register_event(SkEvent event, SkEventCallback cb) {
  event_bus_register(event, cb);
}

void sk_unregister_event(SkEvent event, SkEventCallback cb) {
  event_bus_unregister(event, cb);
}
void sk_close_window(void) {
  SkaldContext_Get()->window.selection_complete = true;
}

#if defined(_WIN32)
#define SK_PATH_SEP '\\'
#else
#define SK_PATH_SEP '/'
#endif

#define SK_CONFIG_MAX_ENTRIES 256
#define SK_CONFIG_NAME_MAX 384
#define SK_CONFIG_HOTKEY_MAX 64
#define SK_CONFIG_ALIAS_MAX 128
#define SK_CONFIG_TARGET_MAX 256
#define SK_CONFIG_QUICKLINK_TARGET_MAX 1024

typedef struct {
  char name[SK_CONFIG_NAME_MAX];
  char hotkey[SK_CONFIG_HOTKEY_MAX];
} SkConfigHotkeyEntry;

typedef struct {
  char target[SK_CONFIG_TARGET_MAX];
  char alias[SK_CONFIG_ALIAS_MAX];
} SkConfigAliasEntry;

typedef struct {
  char name[SK_CONFIG_NAME_MAX];
  char target[SK_CONFIG_QUICKLINK_TARGET_MAX];
} SkConfigQuicklinkEntry;

static SkConfigHotkeyEntry hotkey_entries[SK_CONFIG_MAX_ENTRIES];
static int hotkey_entry_count = 0;

static SkConfigAliasEntry alias_entries[SK_CONFIG_MAX_ENTRIES];
static int alias_entry_count = 0;

static SkConfigQuicklinkEntry quicklink_entries[SK_CONFIG_MAX_ENTRIES];
static int quicklink_entry_count = 0;

static void *sk_config_mutex = NULL;
static char sk_config_path[1024];
static bool sk_config_loaded = false;

static void sk_config_lock(void) {
  if (!sk_config_mutex)
    sk_config_mutex = Platform_MutexCreate();
  if (sk_config_mutex)
    Platform_MutexLock(sk_config_mutex);
}

static void sk_config_unlock(void) {
  if (sk_config_mutex)
    Platform_MutexUnlock(sk_config_mutex);
}

static int hotkey_index_of(const char *name) {
  if (!name)
    return -1;
  for (int i = 0; i < hotkey_entry_count; i++)
    if (sk_string_equals_ci(hotkey_entries[i].name, name))
      return i;
  return -1;
}

static int hotkey_index_by_key(const char *hotkey) {
  if (!hotkey)
    return -1;
  for (int i = 0; i < hotkey_entry_count; i++)
    if (sk_string_equals_ci(hotkey_entries[i].hotkey, hotkey))
      return i;
  return -1;
}

static int alias_index_of(const char *target) {
  if (!target)
    return -1;
  for (int i = 0; i < alias_entry_count; i++)
    if (sk_string_equals_ci(alias_entries[i].target, target))
      return i;
  return -1;
}

static int quicklink_index_of(const char *name) {
  if (!name)
    return -1;
  for (int i = 0; i < quicklink_entry_count; i++)
    if (sk_string_equals_ci(quicklink_entries[i].name, name))
      return i;
  return -1;
}

static void hotkey_entries_remove(int i) {
  for (int j = i; j + 1 < hotkey_entry_count; j++)
    hotkey_entries[j] = hotkey_entries[j + 1];
  memset(&hotkey_entries[hotkey_entry_count - 1], 0,
         sizeof(hotkey_entries[hotkey_entry_count - 1]));
  hotkey_entry_count--;
}

static void quicklink_entries_remove(int i) {
  for (int j = i; j + 1 < quicklink_entry_count; j++)
    quicklink_entries[j] = quicklink_entries[j + 1];
  memset(&quicklink_entries[quicklink_entry_count - 1], 0,
         sizeof(quicklink_entries[quicklink_entry_count - 1]));
  quicklink_entry_count--;
}

static void alias_entries_remove(int i) {
  for (int j = i; j + 1 < alias_entry_count; j++)
    alias_entries[j] = alias_entries[j + 1];
  memset(&alias_entries[alias_entry_count - 1], 0,
         sizeof(alias_entries[alias_entry_count - 1]));
  alias_entry_count--;
}

static void hotkey_entries_set(const char *name, const char *hotkey) {
  int i = hotkey_index_of(name);
  if (i >= 0) {
    snprintf(hotkey_entries[i].hotkey, sizeof(hotkey_entries[i].hotkey), "%s",
             hotkey);
    return;
  }
  if (hotkey_entry_count >= SK_CONFIG_MAX_ENTRIES) {
    log_error("sk_config: hotkey table full");
    return;
  }
  snprintf(hotkey_entries[hotkey_entry_count].name,
           sizeof(hotkey_entries[hotkey_entry_count].name), "%s", name);
  snprintf(hotkey_entries[hotkey_entry_count].hotkey,
           sizeof(hotkey_entries[hotkey_entry_count].hotkey), "%s", hotkey);
  hotkey_entry_count++;
}

static void quicklink_entries_set(const char *name, const char *target) {
  int i = quicklink_index_of(name);
  if (i >= 0) {
    snprintf(quicklink_entries[i].target, sizeof(quicklink_entries[i].target),
             "%s", target);
    return;
  }
  if (quicklink_entry_count >= SK_CONFIG_MAX_ENTRIES) {
    log_error("sk_config: quicklink table full");
    return;
  }
  snprintf(quicklink_entries[quicklink_entry_count].name,
           sizeof(quicklink_entries[quicklink_entry_count].name), "%s", name);
  snprintf(quicklink_entries[quicklink_entry_count].target,
           sizeof(quicklink_entries[quicklink_entry_count].target), "%s",
           target);
  quicklink_entry_count++;
}

static void alias_entries_set(const char *target, const char *alias) {
  int i = alias_index_of(target);
  if (i >= 0) {
    snprintf(alias_entries[i].alias, sizeof(alias_entries[i].alias), "%s",
             alias);
    return;
  }
  if (alias_entry_count >= SK_CONFIG_MAX_ENTRIES) {
    log_error("sk_config: alias table full");
    return;
  }
  snprintf(alias_entries[alias_entry_count].target,
           sizeof(alias_entries[alias_entry_count].target), "%s", target);
  snprintf(alias_entries[alias_entry_count].alias,
           sizeof(alias_entries[alias_entry_count].alias), "%s", alias);
  alias_entry_count++;
}

static bool sk_config_parse_path(void) {
  char dir[512];
  if (!data_dir(dir, sizeof(dir)))
    return false;
  Platform_MkdirIfNotExists(dir);
  snprintf(sk_config_path, sizeof(sk_config_path), "%s%ccore%cskald.toml", dir,
           SK_PATH_SEP, SK_PATH_SEP);
  return true;
}

static bool sk_config_ready(void) {
  return sk_config_path[0] != '\0' || sk_config_parse_path();
}

static bool sk_config_apply_entry(const ConfigTomlEntry *e, void *user) {
  (void)user;
  char body[1024];
  if (e->type != CONFIG_TOML_STRING) {
    log_error("sk_config: skipping non-string value for '%s' in [%s]", e->key,
              e->section);
    return true;
  }
  if (!config_toml_decode_string(e->raw, body, sizeof(body))) {
    log_error("sk_config: bad string for '%s' in [%s]", e->key, e->section);
    return true;
  }
  if (strcmp(e->section, "hotkeys") == 0) {
    HotkeyKey key;
    if (!hotkey_string_parse(body, &key)) {
      log_error("sk_config: unparsable hotkey '%s' for '%s'", body, e->key);
      return true;
    }
    hotkey_entries_set(e->key, body);
  } else if (strcmp(e->section, "aliases") == 0) {
    alias_entries_set(e->key, body);
  } else if (strcmp(e->section, "quicklinks") == 0) {
    quicklink_entries_set(e->key, body);
  } else {
    log_warn("sk_config: unknown section [%s], ignoring", e->section);
  }
  return true;
}

static void sk_config_trim_inplace(char *s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' ||
                   s[n - 1] == '\t'))
    s[--n] = '\0';
}

static void sk_config_migrate_hotkeys_txt(const char *dir) {
  char mkpath[1088];
  snprintf(mkpath, sizeof(mkpath), "%s%ccontrol_center%chotkeys.txt", dir,
           SK_PATH_SEP, SK_PATH_SEP);
  FILE *f = fopen(mkpath, "r");
  if (!f)
    return;

  size_t migrated = 0;
  char line[640];
  while (fgets(line, sizeof(line), f)) {
    char copy[640];
    snprintf(copy, sizeof(copy), "%s", line);
    char *tab = strchr(copy, '\t');
    if (!tab)
      continue;
    *tab = '\0';
    char *hotkey = copy;
    char *action = tab + 1;
    sk_config_trim_inplace(hotkey);
    while (*action == ' ' || *action == '\t')
      action++;
    sk_config_trim_inplace(action);
    if (!hotkey[0] || !action[0])
      continue;
    HotkeyKey key;
    if (!hotkey_string_parse(hotkey, &key))
      continue;
    if (hotkey_index_of(action) >= 0 || hotkey_index_by_key(hotkey) >= 0)
      continue;
    hotkey_entries_set(action, hotkey);
    migrated++;
  }
  fclose(f);
  if (migrated > 0) {
    log_info("sk_config: migrated %zu hotkey(s) from "
             "control_center/hotkeys.txt",
             migrated);
    if (sk_config_ready())
      for (int i = 0; i < hotkey_entry_count; i++)
        config_section_set(sk_config_path, "hotkeys", hotkey_entries[i].name,
                           hotkey_entries[i].hotkey);
  }
}

static void sk_config_init_locked(void) {
  if (sk_config_loaded)
    return;
  sk_config_loaded = true;
  char dir[512];
  if (!data_dir(dir, sizeof(dir)))
    dir[0] = '\0';
  if (!sk_config_parse_path()) {
    log_error("sk_config: data dir unavailable, config persistence disabled");
    return;
  }
  ConfigTomlStatus st =
      config_toml_parse(sk_config_path, sk_config_apply_entry, NULL);
  if (st == CONFIG_TOML_ERR_IO)
    log_info("sk_config: %s not found, starting with empty tables",
             sk_config_path);
  else if (st == CONFIG_TOML_ERR_PARSE)
    log_error("sk_config: parse failed for %s, keeping defaults",
              sk_config_path);

  if (dir[0])
    sk_config_migrate_hotkeys_txt(dir);

  for (int i = 0; i < alias_entry_count; i++)
    sk_register_alias(.alias = alias_entries[i].alias,
                      .target = alias_entries[i].target);
}

void sk_config_init(void) {
  sk_config_lock();
  sk_config_init_locked();
  sk_config_unlock();
}

static void hotkey_bind_live(const char *hotkey, SkConfigFn cb, void *ud) {
  if (!hotkey || !cb)
    return;
  sk_unregister_hotkey(hotkey);
  sk_register_hotkey(.hotkey = (char *)hotkey, .cb_ud = (HotkeyCallbackUd)cb,
                     .ud = ud);
}

bool ski_config_set_hotkey(const SkConfigHotkeySpec *spec) {
  if (!spec || !spec->name || !spec->name[0])
    return false;
  if (spec->name[0] && strlen(spec->name) >= SK_CONFIG_NAME_MAX)
    return false;
  if (!spec->hotkey || !spec->hotkey[0])
    return false;
  if (strlen(spec->hotkey) >= SK_CONFIG_HOTKEY_MAX)
    return false;
  HotkeyKey key;
  if (!hotkey_string_parse(spec->hotkey, &key))
    return false;

  sk_config_lock();
  sk_config_init_locked();
  int displaced = hotkey_index_by_key(spec->hotkey);
  int self = hotkey_index_of(spec->name);
  if (displaced >= 0 && displaced != self) {
    log_warn("sk_config: '%s' overwrites hotkey '%s' previously bound to "
             "'%s'",
             spec->name, spec->hotkey, hotkey_entries[displaced].name);
    hotkey_entries_remove(displaced);
    if (displaced < self)
      self--;
  }

  char old_hotkey[SK_CONFIG_HOTKEY_MAX] = "";
  int i = hotkey_index_of(spec->name);
  if (i >= 0)
    snprintf(old_hotkey, sizeof(old_hotkey), "%s", hotkey_entries[i].hotkey);

  hotkey_entries_set(spec->name, spec->hotkey);
  if (!sk_config_ready() ||
      !config_section_set(sk_config_path, "hotkeys", spec->name, spec->hotkey))
    log_error("sk_config: failed to save %s", sk_config_path);

  if (old_hotkey[0] && !sk_string_equals_ci(old_hotkey, spec->hotkey))
    sk_unregister_hotkey(old_hotkey);
  if (spec->cb)
    hotkey_bind_live(spec->hotkey, spec->cb, spec->ud);
  sk_config_unlock();
  return true;
}

char *sk_config_get_hotkey(const char *name) {
  if (!name)
    return NULL;
  sk_config_lock();
  sk_config_init_locked();
  int i = hotkey_index_of(name);
  char *out = i >= 0 ? strdup(hotkey_entries[i].hotkey) : NULL;
  sk_config_unlock();
  return out;
}

bool sk_config_clear_hotkey(const char *name) {
  if (!name)
    return false;
  sk_config_lock();
  sk_config_init_locked();
  int i = hotkey_index_of(name);
  if (i < 0) {
    sk_config_unlock();
    return false;
  }
  char hk[SK_CONFIG_HOTKEY_MAX];
  snprintf(hk, sizeof(hk), "%s", hotkey_entries[i].hotkey);
  hotkey_entries_remove(i);
  if (sk_config_ready())
    config_section_clear(sk_config_path, "hotkeys", name);
  sk_unregister_hotkey(hk);
  sk_config_unlock();
  return true;
}

bool sk_config_bind_hotkey(const char *name, SkConfigFn cb, void *ud) {
  if (!name || !cb)
    return false;
  sk_config_lock();
  sk_config_init_locked();
  int i = hotkey_index_of(name);
  if (i < 0) {
    sk_config_unlock();
    return false;
  }
  char hk[SK_CONFIG_HOTKEY_MAX];
  snprintf(hk, sizeof(hk), "%s", hotkey_entries[i].hotkey);
  hotkey_bind_live(hk, cb, ud);
  sk_config_unlock();
  return true;
}

int sk_config_hotkey_count(void) {
  sk_config_lock();
  sk_config_init_locked();
  int n = hotkey_entry_count;
  sk_config_unlock();
  return n;
}

char *sk_config_hotkey_name_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < hotkey_entry_count;
  char *out = ok ? strdup(hotkey_entries[index].name) : NULL;
  sk_config_unlock();
  return out;
}

char *sk_config_hotkey_value_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < hotkey_entry_count;
  char *out = ok ? strdup(hotkey_entries[index].hotkey) : NULL;
  sk_config_unlock();
  return out;
}

bool ski_config_set_alias(const SkConfigAliasSpec *spec) {
  if (!spec || !spec->target || !spec->target[0])
    return false;
  if (strlen(spec->target) >= SK_CONFIG_TARGET_MAX)
    return false;
  const char *alias = spec->alias;
  bool clearing = !alias || !alias[0];
  if (!clearing && strlen(alias) >= SK_CONFIG_ALIAS_MAX)
    return false;

  sk_config_lock();
  sk_config_init_locked();
  if (clearing) {
    int i = alias_index_of(spec->target);
    if (i >= 0) {
      const char *cur = sk_alias_for(spec->target);
      if (cur)
        sk_remove_alias(cur);
      alias_entries_remove(i);
      if (sk_config_ready())
        config_section_clear(sk_config_path, "aliases", spec->target);
    } else {
      const char *cur = sk_alias_for(spec->target);
      if (cur)
        sk_remove_alias(cur);
    }
    sk_config_unlock();
    return true;
  }

  alias_entries_set(spec->target, alias);
  if (!sk_config_ready() ||
      !config_section_set(sk_config_path, "aliases", spec->target, alias))
    log_error("sk_config: failed to save %s", sk_config_path);
  sk_register_alias(.alias = alias, .target = spec->target);
  sk_config_unlock();
  return true;
}

char *sk_config_get_alias(const char *target) {
  if (!target)
    return NULL;
  sk_config_lock();
  sk_config_init_locked();
  int i = alias_index_of(target);
  char *out = i >= 0 ? strdup(alias_entries[i].alias) : NULL;
  sk_config_unlock();
  return out;
}

bool sk_config_clear_alias(const char *target) {
  if (!target)
    return false;
  SkConfigAliasSpec spec = {.target = target, .alias = NULL};
  return ski_config_set_alias(&spec);
}

int sk_config_alias_count(void) {
  sk_config_lock();
  sk_config_init_locked();
  int n = alias_entry_count;
  sk_config_unlock();
  return n;
}

char *sk_config_alias_target_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < alias_entry_count;
  char *out = ok ? strdup(alias_entries[index].target) : NULL;
  sk_config_unlock();
  return out;
}

char *sk_config_alias_value_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < alias_entry_count;
  char *out = ok ? strdup(alias_entries[index].alias) : NULL;
  sk_config_unlock();
  return out;
}

int sk_config_quicklink_count(void) {
  sk_config_lock();
  sk_config_init_locked();
  int n = quicklink_entry_count;
  sk_config_unlock();
  return n;
}

char *sk_config_quicklink_name_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < quicklink_entry_count;
  char *out = ok ? strdup(quicklink_entries[index].name) : NULL;
  sk_config_unlock();
  return out;
}

char *sk_config_quicklink_value_at(int index) {
  sk_config_lock();
  sk_config_init_locked();
  bool ok = index >= 0 && index < quicklink_entry_count;
  char *out = ok ? strdup(quicklink_entries[index].target) : NULL;
  sk_config_unlock();
  return out;
}

bool sk_config_set_quicklink(const char *name, const char *target) {
  if (!name || !name[0] || !target || !target[0])
    return false;
  if (strlen(name) >= SK_CONFIG_NAME_MAX ||
      strlen(target) >= SK_CONFIG_QUICKLINK_TARGET_MAX)
    return false;
  sk_config_lock();
  sk_config_init_locked();
  quicklink_entries_set(name, target);
  if (!sk_config_ready() ||
      !config_section_set(sk_config_path, "quicklinks", name, target))
    log_error("sk_config: failed to save %s", sk_config_path);
  sk_config_unlock();
  return true;
}

bool sk_copy_to_clipboard_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_copy_to_clipboard(buf);
}

bool sk_copy_file_path_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_copy_file_path(buf);
}

bool sk_paste_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_paste(buf);
}

SkStringList sk_load_file_list_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_load_file_list(buf);
}

bool sk_file_exists_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_file_exists(buf);
}

bool sk_prewarm_image_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_prewarm_image(buf);
}

bool sk_write_to_file_fmt(const char *lines[], int count, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_write_to_file(buf, lines, count);
}

int sk_get_icon_size_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_get_icon_size(buf);
}

bool sk_launch_detached_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_launch_detached(buf);
}

bool sk_terminate_process_by_name_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_terminate_process_by_name(buf);
}

bool sk_process_running_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_process_running(buf);
}

void sk_send_hotkey_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_send_hotkey(buf);
}

SkKey sk_hotkey_from_string_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_hotkey_from_string(buf);
}

bool sk_launch_browser_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_launch_browser(buf);
}

bool sk_query_browser_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_query_browser(buf);
}

bool sk_focus_window_by_title_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_focus_window_by_title(buf);
}

SkWindowHandle sk_get_window_id_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_get_window_id(buf);
}

SkWindowHandle sk_get_window_by_root_atom_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_get_window_by_root_atom(buf);
}

bool sk_window_set_long_property_fmt(SkWindowHandle window, long value, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_window_set_long_property(window, buf, value);
}

bool sk_window_send_message_fmt(SkWindowHandle window, long data, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_window_send_message(window, buf, data);
}

bool sk_dismiss_message_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_dismiss_message(buf);
}

bool sk_message_active_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_message_active(buf);
}

bool sk_close_windows_by_title_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_close_windows_by_title(buf);
}

bool sk_close_windows_by_id_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_close_windows_by_id(buf);
}

bool sk_set_theme_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_set_theme(buf);
}

void sk_set_edit_text_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_set_edit_text(buf);
}

bool sk_checkbox_is_checked_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_checkbox_is_checked(buf);
}

bool sk_checkbox_is_hovered_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_checkbox_is_hovered(buf);
}

bool sk_checkbox_is_clicked_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_checkbox_is_clicked(buf);
}

bool sk_button_is_checked_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_button_is_checked(buf);
}

bool sk_button_is_hovered_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_button_is_hovered(buf);
}

bool sk_button_is_clicked_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_button_is_clicked(buf);
}

bool sk_slider_is_hovered_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_is_hovered(buf);
}

bool sk_slider_is_clicked_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_is_clicked(buf);
}

bool sk_slider_is_dragging_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_is_dragging(buf);
}

float sk_slider_value_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_value(buf);
}

float sk_slider_fraction_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_fraction(buf);
}

int sk_slider_percent_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_percent(buf);
}

bool sk_slider_set_value_fmt(float value, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_slider_set_value(buf, value);
}

SkPos sk_get_widget_pos_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_get_widget_pos(buf);
}

int sk_get_text_width_fmt(int fontsize, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_get_text_width(buf, fontsize);
}

void sk_http_get_async_fmt(SkHttpCallback cb, void *userdata, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_http_get_async(buf, cb, userdata);
}

SkFlagList sk_parse_flags_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_parse_flags(buf);
}

SkLibrary sk_library_open_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_library_open(buf);
}

void sk_library_register_fmt(SkLibrary handle, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_library_register(buf, handle);
}

SkLibrary sk_library_get_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_library_get(buf);
}

void sk_hashmap_put_fmt(SkHashMap **map, long value, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_hashmap_put(map, buf, value);
}

void sk_hashmap_add_fmt(SkHashMap **map, long delta, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_hashmap_add(map, buf, delta);
}

long sk_hashmap_get_fmt(SkHashMap *map, long def, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_hashmap_get(map, buf, def);
}

bool sk_hashmap_has_fmt(SkHashMap *map, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_hashmap_has(map, buf);
}

bool sk_hashmap_del_fmt(SkHashMap **map, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_hashmap_del(map, buf);
}

bool sk_is_window_open_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_is_window_open(buf);
}

bool sk_type_text_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_type_text(buf);
}

bool sk_config_clear_hotkey_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_config_clear_hotkey(buf);
}

bool sk_config_bind_hotkey_fmt(SkConfigFn cb, void *ud, const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_config_bind_hotkey(buf, cb, ud);
}

bool sk_config_clear_alias_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_config_clear_alias(buf);
}

bool sk_hotkey_taken_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  return sk_hotkey_taken(buf);
}

void sk_reload_feature_fmt(const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_reload_feature(buf);
}

void sk_cap_register_abi_fmt(unsigned abi, void *fn, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_cap_register_abi(buf, abi, fn);
}

void sk_cap_bind_declare_fmt(void *consumer, const char *fmt, ...) {
  SK_FMT_BUFFER();
  sk_cap_bind_declare(buf, consumer);
}

