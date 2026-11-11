#include <stdint.h>
#include "helpers/helpers_fs.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/checkbox.h"
#include "core/types/file.h"
#include "core/types/prompt.h"
#include "core/widget_registry.h"
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_ui.h"
#include "core/window_manager.h"
#include "core/thread_registry.h"
#include "platform/lib.h"
#include "core/clipboard_history.h"
#include "core/skapi.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_http.h"
#include "helpers/helpers_ui.h"
#include "core/prompt_geometry.h"
#include "core/feature_loader.h"
#include "core/feature_path.h"
#include "core/cap_registry.h"
#include "helpers/helpers_owner.h"
#include "platform/widget.h"
#include "core/hover_widget.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/hotkey_string.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "helpers/helpers_render_thread.h"
#include "deps/stb_ds.h"
#include "config_values.h"
#include "core/cc_registry.h"
#include "core/skapi_input.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/skapi_types.h"
#include "core/types/keys.h"
#include "core/types/window.h"
#include "core/feature_loader.h"

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

bool sk_get_all_files_from_dir(const char *dir_path,
                               char **list_of_allowed_extensions,
                               ListOfFiles *list_of_files,
                               char **list_of_names_to_ignore,
                               bool strip_extension, int max_depth) {

  if (!dir_path || !list_of_files)
    return false;

  char expanded_dir[MAX_PATH];
  if (!ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir)))
    return false;
  dir_path = expanded_dir;
  return FileWalk_AllFilesFromDir(dir_path, list_of_allowed_extensions,
                                  list_of_files, list_of_names_to_ignore,
                                  strip_extension, max_depth);
}

char *sk_search_file_in_dir(const char *directory, const char *search_string,
                            char *output_path) {

  if (!directory)
    return NULL;

  char expanded_dir[MAX_PATH];
  if (!ExpandPath(directory, expanded_dir, sizeof(expanded_dir)))
    return NULL;
  directory = expanded_dir;
  return sk_search_file_in_dir_platform(directory, search_string, output_path);
}

bool sk_launch_with_args(const char *path, const char *args) {
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded))) {
    return false;
  }
  bool r = sk_launch_with_args_platform(expanded, args);
  return r;
}

bool sk_launch_with_args_as_admin(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
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
  FeatureLoader_ManifestAddDepends(owner_file, dep);
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

SkStringList sk_load_file_list(const char *filepath) {
  SkStringList list = {0};
  if (!filepath)
    return list;
  char full[MAX_PATH];
  if (!FeaturePath_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                   sizeof(full))) {

    log_warn("sk_load_file_list resolve failed name='%s' feature='%s'",
             filepath,
             SkaldContext_Get()->window.current_feature
                 ? SkaldContext_Get()->window.current_feature
                 : "(null)");
    return list;
  }
  list = File_ReadLines(full);
  return list;
}

long sk_load_file(const char *filepath, char *buffer, long buffer_size) {
  if (!filepath || !buffer || buffer_size <= 0)
    return -1;
  buffer[0] = '\0';
  char full[MAX_PATH];
  if (!FeaturePath_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                   sizeof(full))) {

    log_warn("sk_load_file resolve failed name='%s' feature='%s'", filepath,
             SkaldContext_Get()->window.current_feature
                 ? SkaldContext_Get()->window.current_feature
                 : "(null)");
    return -1;
  }
  return File_ReadAll(full, buffer, buffer_size);
}

bool sk_free_list(const char *list[], int count) {
  if (!list || count <= 0)
    return false;
  for (int i = 0; i < count; i++)
    free((void *)list[i]);
  return true;
}

void sk_free_file_list(SkStringList *list) {
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
  if (!FeaturePath_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                   sizeof(full)))
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
  if (!FeaturePath_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                   sizeof(full)))
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

bool sk_write_temp_file(const char *name, const void *data, size_t len,
                        char *out_path, size_t out_len) {
  return TempFile_Write(name, data, len, out_path, out_len);
}

char *sk_read_file_to_string(const char *path, size_t *out_len) {
  if (!path)
    return NULL;
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    strncpy(expanded, path, sizeof(expanded) - 1);
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

long sk_read_entire_file(const char *full_path, char *buffer,
                         long buffer_size) {
  return File_ReadAll(full_path, buffer, buffer_size);
}

bool sk_write_string_to_file(const char *path, const char *content) {
  if (!path || !content)
    return false;
  char full[MAX_PATH];
  if (FeaturePath_ResolveDataPath(__builtin_return_address(0), path, full,
                                  sizeof(full))) {
    path = full;
  } else {
    char expanded[MAX_PATH];
    if (!ExpandPath(path, expanded, sizeof(expanded)))
      strncpy(expanded, path, sizeof(expanded) - 1);
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
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    strncpy(expanded, path, sizeof(expanded) - 1);
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
      sk_free_file_list(&files);
      return true;
    }
  bool ok = sk_append_to_file(path, line);
  sk_free_file_list(&files);
  return ok;
}

bool sk_remove_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  SkStringList files = sk_load_file_list(path);
  const char **kept =
      (const char **)malloc(sizeof(char *) * (files.count ? files.count : 1));
  if (!kept) {
    sk_free_file_list(&files);
    return false;
  }
  int k = 0;
  for (size_t i = 0; i < files.count; i++) {
    if (strcmp(files.items[i], line) != 0)
      kept[k++] = files.items[i];
    else {
      free(files.items[i]);
      files.items[i] = NULL;
    }
  }
  bool ok = sk_write_to_file(path, kept, k);
  free(kept);
  sk_free_file_list(&files);
  return ok;
}

bool sk_remove_line_contains(const char *path, const char *substr) {
  if (!path || !substr)
    return false;
  SkStringList files = sk_load_file_list(path);
  const char **kept =
      (const char **)malloc(sizeof(char *) * (files.count ? files.count : 1));
  if (!kept) {
    sk_free_file_list(&files);
    return false;
  }
  int k = 0;
  for (size_t i = 0; i < files.count; i++) {
    if (!strstr(files.items[i], substr))
      kept[k++] = files.items[i];
    else {
      free(files.items[i]);
      files.items[i] = NULL;
    }
  }
  bool ok = sk_write_to_file(path, kept, k);
  free(kept);
  sk_free_file_list(&files);
  return ok;
}

bool sk_parse_desktop_file(const char *path, char *out_name, size_t n,
                           bool *out_nodisplay) {

  if (!path || !out_name)
    return false;
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    strncpy(expanded, path, sizeof(expanded) - 1);
  FILE *f = fopen(expanded, "r");
  if (!f)
    return false;
  char line[1024];
  bool found = false;
  if (out_nodisplay)
    *out_nodisplay = false;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (String_ContainsCI(p, "NoDisplay=true") ||
        String_ContainsCI(p, "Hidden=true")) {
      if (out_nodisplay)
        *out_nodisplay = true;
    }
    if (!found && String_StartsWithCI(p, "Name=")) {
      p += 5;
      while (*p == ' ' || *p == '\t')
        p++;
      p[strcspn(p, "\r\n")] = '\0';
      strncpy(out_name, p, n - 1);
      out_name[n - 1] = '\0';
      found = true;
    }
  }
  fclose(f);
  return found;
}

bool ski_register_remapping(const SkRemappingSpec *spec) {
  if (!spec || !spec->src || !spec->dst)
    return false;
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char *const *windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  return remap_register_internal(spec->src, spec->dst, whitelist, windows);
}

static void sk_snippet_fire(int idx, int del) {

  const SnippetBinding *b = snippet_get_binding(idx);
  if (!b || !b->trigger) {
    return;
  }
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
  snippet_set_fire_fn(sk_snippet_fire);
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char *const *windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
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
  if ((!path_a || !path_a[0]) && (!path_b || !path_b[0]))
    return;
  int n = 0;
  for (int i = 0; i < feature_order_count; i++) {
    if (Owner_ModuleMatches(feature_order[i]->owner, path_a, path_b)) {
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
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char *const *bw = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  HotkeyBinding binding = convert_into_hotkey_binding(
      (char *)spec->hotkey, spec->cb, NULL, bw, whitelist, spec->ud);
  binding.hotkey_callback_ud = spec->cb_ud;
  binding.hold = true;
  binding.should_swallow = spec->should_swallow;
  return hotkey_register_internal(binding);
}

bool ski_unregister_hotkey(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyBinding binding = convert_into_hotkey_binding((char *)hotkey, NULL,
                                                      NULL, NULL, false, NULL);
  return hotkey_unregister_internal(binding.vk_code, binding.modifiers);
}

void sk_disable_global_hotkeys(void) { hotkey_set_suspended(true); }
void sk_enable_global_hotkeys(void) { hotkey_set_suspended(false); }

int sk_hotkey_count(void) { return hotkey_count(); }

static bool hotkey_binding_format(const HotkeyBinding *b, char *out,
                                  size_t cap) {
  if (!b || !out || cap == 0)
    return false;
  HotkeyKey key = {.vk_code = b->vk_code, .mods = b->modifiers};
  if (!HotkeyString_Format(key, out, (int)cap)) {
    out[0] = '\0';
    return false;
  }
  return true;
}

bool sk_hotkey_string_at(int index, char *out, size_t cap) {
  return hotkey_binding_format(hotkey_get(index), out, cap);
}

bool sk_hotkey_for_ud(void *ud, char *out, size_t cap) {
  return hotkey_binding_format(hotkey_find_by_ud(ud), out, cap);
}

void sk_register_deferred_init(void (*cb)(void)) { DeferredInit_Register(cb); }

bool sk_hotkey_for_callback(void (*cb)(void), char *out, size_t cap) {
  return hotkey_binding_format(hotkey_find_by_callback((HotkeyCallback)cb), out,
                               cap);
}

bool sk_hotkey_taken(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyKey key = {0};
  if (!HotkeyString_Parse(hotkey, &key))
    return false;
  return hotkey_is_taken(key.vk_code, key.mods);
}

int sk_get_feature_list(const char **out_names, void (**out_cbs)(void),
                        int max) {

  int n = feature_order_count < max ? feature_order_count : max;
  for (int i = 0; i < n; i++) {
    if (out_names)
      out_names[i] = feature_order[i]->name;
    if (out_cbs)
      out_cbs[i] = feature_order[i]->cb;
  }
  return n;
}

int sk_get_feature_list_with_icons(const char **out_names,
                                   void (**out_cbs)(void),
                                   const char ***out_icons, int max) {

  int n = feature_order_count < max ? feature_order_count : max;
  for (int i = 0; i < n; i++) {
    if (out_names)
      out_names[i] = feature_order[i]->name;
    if (out_cbs)
      out_cbs[i] = feature_order[i]->cb;
  }
  static const char **icon_ptrs = NULL;
  static int icon_ptrs_cap = 0;
  if (n > icon_ptrs_cap) {
    const char **new_ptrs =
        (const char **)realloc(icon_ptrs, (size_t)n * sizeof(const char *));
    if (new_ptrs) {
      icon_ptrs = new_ptrs;
      icon_ptrs_cap = n;
    }
  }
  if (icon_ptrs) {
    for (int i = 0; i < n; i++)
      icon_ptrs[i] = feature_order[i]->icon[0] ? feature_order[i]->icon : NULL;
    if (out_icons)
      *out_icons = icon_ptrs;
  } else {
    if (out_icons)
      *out_icons = NULL;
  }
  return n;
}

char *sk_get_icon_path(const char *path) {
  if (!path)
    return NULL;
  return Platform_GetIconPath((char *)path);
}

int sk_get_icon_size(const char *icon_path) {
  if (!icon_path)
    return 20;
  return Platform_GetIconSize((char *)icon_path);
}

char *sk_run_terminal_command(char *command) {
  if (!command) {
    return NULL;
  }
  char *r = Platform_RunTerminalCommand(command, false);
  return r;
}

char *sk_run_argv(const char *const argv[], const char *stdin_data,
                  int *out_status) {

  if (!argv || !argv[0])
    return NULL;
  return Platform_RunArgvCapture(argv, stdin_data, out_status);
}

char *sk_run_terminal_command_blocking(char *command) {
  if (!command)
    return NULL;
  return Platform_RunCommandAndGetOutput(command, PLATFORM_RUN_PRIO_NORMAL);
}

char *sk_run_terminal_command_and_open(char *command) {
  if (!command) {
    return NULL;
  }
  char *r = Platform_RunTerminalCommand(command, true);
  return r;
}

bool sk_launch_detached(char *command) {
  if (!command) {
    return false;
  }
  bool r = Platform_LaunchDetached(command);
  return r;
}

bool sk_launch_terminal_at_path(char *path, char *args) {
  bool r = false;
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded))) {
    return false;
  }
  r = Platform_LaunchTerminalAtPath(expanded, args);
  return r;
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
  return String_ContainsCI(haystack, needle);
}

char *sk_string_cut_at(const char *str, char delim) {
  return String_CutAt(str, delim);
}
char *sk_string_cut_before(const char *str, char delim) {
  return String_CutBefore(str, delim);
}
char *sk_string_cut_after(const char *str, char delim) {
  return String_CutAfter(str, delim);
}
char *sk_string_between(const char *str, char open, char close) {
  return String_Between(str, open, close);
}
char *sk_string_inside(const char *str, char delim) {
  return String_Inside(str, delim);
}
char *sk_string_trim(const char *str) { return String_Trim(str); }
char *sk_string_to_lower(const char *str) { return String_ToLower(str); }
char *sk_string_to_upper(const char *str) { return String_ToUpper(str); }
bool sk_string_starts_with(const char *str, const char *prefix) {
  return String_StartsWith(str, prefix);
}
bool sk_string_ends_with(const char *str, const char *suffix) {
  return String_EndsWith(str, suffix);
}
char *sk_string_replace(const char *str, const char *from, const char *to) {
  return String_Replace(str, from, to);
}
char *sk_string_slice(const char *str, int start, int end) {
  return String_Slice(str, start, end);
}

StringList sk_string_split(const char *str, const char *delimiter) {
  StringList empty = {NULL, 0};
  if (!str || !delimiter || !delimiter[0])
    return empty;
  return String_Split(str, delimiter);
}

void sk_string_list_free(StringList *list) { StringList_Free(list); }

void sk_populate_listbox(ListBoxInfo *lb, const char *filter,
                         const char **items, int count) {
  PopulateListBox(lb, filter, items, count, NULL);
}

void sk_populate_listbox_ex(ListBoxInfo *lb, const char *filter,
                            const char **items, int count,
                            bool (*filter_match)(const char *item,
                                                 const char *filter)) {
  PopulateListBox(lb, filter, items, count, filter_match);
}

bool sk_is_selected(ListBoxInfo *lb) {
  if (!lb)
    return false;
  return UI_IsListBoxSelected(lb);
}

void sk_set_edit_text(const char *text) {
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx) {
    return;
  }
  UI_SetText(text ? text : "");
}

void sk_set_edit_cursor(int position) {
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx) {
    return;
  }
  UI_SetCursorPosition(position);
}

int sk_get_edit_cursor(void) {
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx)
    return 0;
  int pos = UI_GetCursorPosition();
  return pos;
}

void sk_get_edit_text(char *buffer, size_t cap) {
  if (!buffer || cap == 0) {
    return;
  }
  buffer[0] = '\0';
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx) {
    return;
  }
  UI_GetCurrentTextEdit(buffer, cap);
}

static void sk_hover_popup_frame(void *ud) {

  SkHoverPopupCtx *p = (SkHoverPopupCtx *)ud;
  if (!p || !p->render) {
    return;
  }
  p->frame++;
  SetDrawOffset(0, 0);
  BeginMeasure();
  p->render();
  int mw = 0, mh = 0;
  EndMeasure(&mw, &mh);
  if (mw > 0 && mh > 0) {
    p->w = mw + p->pad;
    p->h = mh + p->pad;
  }
}

static SkHoverPopupCtx *sk_hover_popup_ctx_new(const SkMessageSpec *spec) {

  SkHoverPopupCtx *p = (SkHoverPopupCtx *)calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->render = spec->render;
  p->w = spec->w > 0 ? spec->w : WIDGET_PROVISIONAL_W;
  p->h = spec->h > 0 ? spec->h : WIDGET_PROVISIONAL_H;
  p->created_at = sk_start_timer();
  p->pad = WIDGET_DEFAULT_PAD;
  return p;
}

static void sk_hover_popup_ctx_free(void *ud) {
  SkHoverPopupCtx *p = (SkHoverPopupCtx *)ud;
  if (p)
    free(p);
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
      SkHoverPopupCtx *ctx = sk_hover_popup_ctx_new(spec);
      if (ctx) {
        HoverWidgetShow((PlHoverWidgetShowSpec){
            .key = spec->hover_key,
            .render = sk_hover_popup_frame,
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
            .release = sk_hover_popup_ctx_free,
            .release_ud = ctx});
      }
      return;
    }
    Platform_HoverMessage((PlHoverMessageSpec){.key = spec->hover_key,
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
  MessageSlot slot = spec->slot ? spec->slot : FREE;
  if (slot == AUTO) {
    slot = IsPopupActive(WH_NULL) ? TOP_CENTER_RELATIVE : TOP_CENTER;
  } else if (slot == FREE) {
    Platform_DisplayMessage(
        (PlDisplayMessageSpec){.message = spec->message,
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
                               .name = spec->name});
    return;
  }
  DisplayMessageSlot(spec->message, slot, mw, mh, spec->prompt_gap, timeout,
                     color, spec->render, spec->name);
}

bool sk_launch_browser(char *url) {
  if (!url) {
    return false;
  }
  bool r = Platform_LaunchBrowserAtUrl(url, false);
  return r;
}

bool sk_query_browser(const char *query) {
  if (!query)
    return false;
  return Platform_QueryBrowser(query);
}

char *sk_get_last_clipboard_entry() {
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

bool sk_paste_from_clipboard() {
  char *content = Platform_GetClipboardEntry();
  return Platform_PasteContent(content);
}

bool sk_paste(char *content) {
  if (!content)
    return false;
  return Platform_PasteContent(content);
}

static bool sk_popup_enter(PromptState *state);

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
  pspec.enter = sk_popup_enter;
  pspec.x = x;
  pspec.y = y;
  pspec.w = w;
  pspec.h = h;
  {
    SkaldContext *ctx = SkaldContext_Get();
    if (ctx)
      ctx->window.editbox_placed = false;
  }
  EventBus_Trigger(EVENT_PROMPT_OPEN);
  char *r = PromptUserRun(wm, &pspec);
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

    sk_free_file_list(&files);
    return false;
  }
  free(files.items[line_index]);
  files.items[line_index] = NULL;
  files.items[line_index] = strdup(new_text);
  if (!files.items[line_index]) {

    sk_free_file_list(&files);
    return false;
  }
  bool ok = sk_write_to_file(filepath, (const char **)files.items, files.count);
  sk_free_file_list(&files);
  return ok;
}

bool sk_focus_window_by_title(char *title) {
  if (!title) {
    return false;
  }
  bool r = Platform_FocusWindow(title);
  return r;
}

bool sk_close_active_window(void) {
  bool r = Platform_CloseActiveWindow();
  return r;
}

bool sk_dismiss_message(const char *name) {
  bool r = sk_dismiss_message_platform(name);
  return r;
}

bool sk_message_active(const char *name) {
  bool r = Platform_MessageActive(name);
  return r;
}

bool sk_toggle_fullscreen_active(void) {
  bool r = Platform_ToggleFullscreenActive();
  return r;
}

bool sk_move_active_window_to_monitor(bool right) {
  bool r = Platform_MoveActiveWindowToMonitor(right);
  return r;
}

bool sk_move_active_window_left(void) {
  bool r = Platform_MoveActiveWindowLeft();
  return r;
}

bool sk_move_active_window_right(void) {
  bool r = Platform_MoveActiveWindowRight();
  return r;
}

bool sk_focus_window_left(void) {
  bool r = Platform_FocusWindowLeft();
  return r;
}

bool sk_focus_window_right(void) {
  bool r = Platform_FocusWindowRight();
  return r;
}

bool sk_move_active_window_up(void) {
  bool r = Platform_MoveActiveWindowUp();
  return r;
}

bool sk_move_active_window_down(void) {
  bool r = Platform_MoveActiveWindowDown();
  return r;
}

bool sk_focus_window_up(void) {
  bool r = Platform_FocusWindowUp();
  return r;
}

bool sk_focus_window_down(void) {
  bool r = Platform_FocusWindowDown();
  return r;
}

bool sk_toggle_minimized_active(void) {
  bool r = Platform_MinimizeActiveWindow();
  return r;
}

bool sk_reload_config(void) { return Config_Reload(); }

static bool sk_popup_enter(PromptState *state) {
  (void)state;
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
  pspec.enter = sk_popup_enter;
  pspec.x = spec->x;
  pspec.y = spec->y;
  pspec.w = spec->w ? spec->w : 600;
  pspec.h = spec->h ? spec->h : 70;
  char *r = WindowManager_ShowPrompt(wm, &pspec);
  return r ? strdup(r) : NULL;
}

static __thread ListBoxInfo *sk_list_lb;
static __thread PlListSpec sk_list_spec;
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
  Item_GetColors((PlGetItemColorsSpec){.idx = idx,
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
  if (col == 0) {
    unsigned long cur = sk_listbox_get_current_standard_color();
    col = cur ? cur : rgb_to_hex(COLOR_TEXT);
  } else {
    col = rgb_to_hex(col);
  }
  int draw_x = spec->x - (spec->no_hscroll ? 0 : ListBox_GetRowHScroll());
  const char *align = spec->align ? spec->align : "CENTER";
  FontHandle fh = spec->font;
  bool r =
      Platform_DrawTextRaw((PlDrawTextRawSpec){.x = draw_x,
                                               .y = spec->y,
                                               .text = real_text,
                                               .color = col,
                                               .font = fh,
                                               .align = (char *)align,
                                               .pad = spec->pad,
                                               .fontsize = spec->fontsize});
  return r;
}

typedef int (*SkWrapMeasureFn)(void *ctx, const char *s, int n);
typedef void (*SkWrapDrawFn)(void *ctx, int x, int y, const char *s, int n);
typedef struct {
  unsigned long color;
  int fontsize;
} SkWrapCtx;

static int SkUtf8LeadLen(unsigned char c) {

  if ((c & 0x80) == 0)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

static void SkWrapRecord(SkCharPos *out, int maxout, int *count, int x, int y) {
  int i = *count;
  (*count)++;
  if (out && i < maxout) {
    out[i].x = x;
    out[i].y = y;
  }
}

static int SkWrapPlaceWord(SkWrapMeasureFn measure, SkWrapDrawFn draw,
                           void *ctx, const char *text, int ws, int we, int wx,
                           int rowtop, SkCharPos *out, int maxout, int *count) {

  int i = ws;
  while (i < we) {
    int j = i + SkUtf8LeadLen((unsigned char)text[i]);
    if (j > we)
      j = we;
    int px = wx + measure(ctx, text + ws, i - ws);
    for (int m = i; m < j; m++)
      SkWrapRecord(out, maxout, count, px, rowtop);
    i = j;
  }
  draw(ctx, wx, rowtop, text + ws, we - ws);
  return measure(ctx, text + ws, we - ws);
}

static int SkWrapLayout(const char *text, int x0, int y0, int max_width,
                        int line_height, SkWrapMeasureFn measure,
                        SkWrapDrawFn draw, void *ctx, SkCharPos *out,
                        int maxout) {

  int count = 0;
  int len = (int)strlen(text);
  int avail = max_width;
  if (avail < 40)
    avail = 40;
  int spaceW = measure(ctx, " ", 1);
  int row = 0;
  int p = 0;
  int lw = 0;
  bool has = false;
  while (p < len) {
    if (text[p] == '\n') {
      SkWrapRecord(out, maxout, &count, x0, y0 + (row + 1) * line_height);
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
      SkWrapRecord(out, maxout, &count, x0 + (has ? lw : 0),
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
          n = SkUtf8LeadLen((unsigned char)text[c]);
          if (n > we - c)
            n = we - c;
        }
        if (c + n >= we) {
          lw = SkWrapPlaceWord(measure, draw, ctx, text, c, we, x0, rowtop, out,
                               maxout, &count);
          has = true;
          c = we;
          break;
        }
        SkWrapPlaceWord(measure, draw, ctx, text, c, c + n, x0, rowtop, out,
                        maxout, &count);
        row++;
        rowtop = y0 + row * line_height;
        c += n;
      }
      p = we;
      continue;
    }
    int add = ww + (has ? spaceW : 0);
    if (has && lw + add > avail) {
      row++;
      rowtop = y0 + row * line_height;
      lw = 0;
      has = false;
    }
    int wx = x0 + (has ? lw + spaceW : 0);
    int pxw = SkWrapPlaceWord(measure, draw, ctx, text, ws, we, wx, rowtop, out,
                              maxout, &count);
    lw = has ? lw + spaceW + pxw : pxw;
    has = true;
    p = we;
  }
  return count;
}

static int SkWrapMeasureThunk(void *ctx, const char *s, int n) {

  SkWrapCtx *c = (SkWrapCtx *)ctx;
  return Platform_MeasureTextWidth(
      (PlMeasureTextWidthSpec){.text = s, .len = n, .fontsize = c->fontsize});
}

static void SkWrapNoDraw(void *ctx, int x, int y, const char *s, int n) {
  (void)ctx;
  (void)x;
  (void)y;
  (void)s;
  (void)n;
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
  int r = SkWrapLayout(spec->text, spec->x, spec->y, spec->max_width,
                       line_height, SkWrapMeasureThunk, SkWrapNoDraw, &ctx,
                       spec->out_positions, cap);
  return r;
}

bool ski_place_editbox(const SkEditSpec *spec) {
  if (!spec) {
    return false;
  }
  SkaldContext_Get()->window.editbox_placed = true;
  bool r = sk_place_editbox_platform(spec);
  return r;
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
      FeaturePath_ResolveAssetPath(__builtin_return_address(0), path, asset,
                                   sizeof(asset))) {
    path = asset;
  }
  bool r = Platform_DrawImageAt(path, spec->x, spec->y, w, h);
  return r;
}

bool ski_draw_rect(const PlRect *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  bool transparent = spec->transparent;
  if (col == 0)
    col = COLOR_TEXT;
  col = rgb_to_hex(col);
  bool r = Platform_DrawRectRaw((PlDrawRectRawSpec){
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .color = col,
      .radius = spec->radius ? spec->radius : 0,
      .rounded = spec->rounded ? spec->rounded : RoundedCorners,
      .opaque = !transparent,
      .thickness = spec->thickness,
      .filled = spec->filled});
  return r;
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

int sk_get_text_height(const char *text) {
  (void)text;
  int lh = 0, rh = 0, lt = 0;
  Platform_GetTextLayoutMetrics(&lh, &rh, &lt);
  int r = lh > 0 ? lh : FontSize;
  return r;
}

SkRect ski_text_bounds(const SkTextBoundsSpec *spec) {
  SkRect r = {.x = 0, .y = 0, .w = 0, .h = 0, .color = 0};
  if (!spec || !spec->text || !spec->text[0])
    return r;
  int fs = spec->fontsize > 0 ? spec->fontsize : FontSize;
  const char *a = spec->align ? spec->align : "CENTER";
  int w = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = spec->text, .len = (int)strlen(spec->text), .fontsize = fs});
  int h = Platform_GetTextLineHeight(fs);
  int base = spec->pad > 0 ? spec->pad : spec->x;
  if (String_ContainsCI(a, "LEFT"))
    r.x = base;
  else if (String_ContainsCI(a, "RIGHT"))
    r.x = base - w;
  else
    r.x = base - w / 2;
  r.y = spec->y;
  r.w = w;
  r.h = h;
  return r;
}

int sk_get_text_width(const char *text, int fontsize) {
  if (!text)
    return 0;
  return Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = text, .len = (int)strlen(text), .fontsize = fontsize});
}

char *sk_http_get(const char *url) { return Platform_HttpGet(url); }

char *sk_http_get_limited(const char *url, size_t max_bytes, size_t *out_len) {
  return Platform_HttpGetLimited(url, max_bytes, out_len);
}

bool sk_http_download(const char *url, const char *save_path) {
  return Platform_HttpDownload(url, save_path);
}

char *sk_http_post(const char *url, const char *content_type,
                   const char *body) {
  return Platform_HttpPost(url, content_type, body);
}

void sk_http_get_async(const char *url, SkHttpCallback cb, void *userdata) {
  Http_GetAsync(url, cb, userdata);
  return;
}

bool ski_draw_line(const PlLine *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  col = rgb_to_hex(col);
  if (col == 0)
    col = COLOR_TEXT;
  bool r =
      Platform_DrawLineRaw((PlDrawLineRawSpec){.x1 = spec->x1,
                                               .y1 = spec->y1,
                                               .x2 = spec->x2,
                                               .y2 = spec->y2,
                                               .color = col,
                                               .thickness = spec->thickness});
  return r;
}

bool ski_draw_circle(const PlCircle *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  col = rgb_to_hex(col);
  if (col == 0)
    col = COLOR_TEXT;
  bool r = Platform_DrawCircleRaw(
      (PlDrawCircleRawSpec){.cx = spec->x,
                            .cy = spec->y,
                            .radius = spec->radius,
                            .color = col,
                            .thickness = spec->thickness});
  return r;
}

static bool sk_listbox_begin_common(const PlListSpec *spec) {
  if (!spec)
    return false;
  ListBoxInfo *lb = spec->lb;
  if (!lb) {
    if (!spec->items || spec->count <= 0)
      return false;
    return false;
  }
  int item_h = spec->item_height ? spec->item_height : 0;
  ListboxBackendInit bi;
  if (!Platform_ListboxBackendBegin(item_h, &bi))
    return false;
  int itemH = item_h > 0 ? item_h : bi.item_h;
  PromptState *st = Platform_ListboxBackendPromptState(lb);
  ListboxLayout lay;
  Listbox_ComputeLayout(spec, bi.list_top, itemH, st ? st->width : 900,
                        st ? st->height : 640, lb->item_count, &lay);
  sk_list_spec = *spec;
  sk_list_lb = lb;
  sk_list_itemH = itemH;
  sk_list_y0 = lay.y0;
  sk_list_bx = lay.bx;
  sk_list_bw = lay.bw;
  sk_list_fw = lay.fw;
  sk_list_fh = lay.fh;
  sk_list_h = lay.list_h;
  sk_list_visible = lay.visible;
  sk_list_cur = 0;
  UI_LB_SetVisibleCount(lay.visible);
  ListBox_ApplyPendingScroll(lb, lay.visible);
  ListBox_SetGeometry(sk_list_y0, sk_list_itemH, lay.visible);
  ListBox_HoverFocus(lb, sk_list_y0, sk_list_itemH, lb->view_offset,
                     lay.visible);
  return true;
}

static bool sk_listbox_next_common(SkListItemCtx *out) {
  if (!out || !sk_list_lb)
    return false;
  if (sk_list_cur >= sk_list_visible)
    return false;
  int idx = sk_list_lb->view_offset + sk_list_cur;
  if (idx < 0 || idx >= sk_list_lb->item_count)
    return false;
  int y = sk_list_y0 + sk_list_cur * sk_list_itemH;
  if (sk_list_cur == 0)
    DrawPromptSeparator(
        (PlDrawPromptSeparatorSpec){.y = sk_list_y0, .width = sk_list_fw});
  bool sel = (idx == sk_list_lb->selected_index);
  Platform_ListboxBackendDrawSelection(sk_list_bx, sk_list_bw, y, sk_list_itemH,
                                       sk_list_fw, sk_list_fh, sel);
  ListBox_SetRowHScroll(sel ? sk_list_lb->text_hscroll : 0);
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
  ListBox_SetRowHScroll(0);
  if (PromptListBottomMargin() > 0 && sk_list_y0 >= 0) {
    DrawPromptSeparator((PlDrawPromptSeparatorSpec){.y = sk_list_y0 + sk_list_h,
                                                    .width = sk_list_fw});
  }
  Platform_ListboxBackendEnd();
  sk_list_lb = NULL;
  sk_list_visible = 0;
  sk_list_cur = 0;
  sk_list_y0 = -1;
}

bool sk_listbox_begin(const SkListSpec *spec) {
  if (!spec)
    return false;
  return sk_listbox_begin_common(spec);
}

bool sk_listbox_next(SkListItemCtx *out) {
  if (!out)
    return false;
  return sk_listbox_next_common(out);
}

int sk_listbox_get_count(void) {
  return sk_list_lb ? sk_list_lb->item_count : 0;
}

bool ski_place_listbox(const SkListSpec *spec) {
  if (!spec) {
    return false;
  }
  if (!sk_listbox_begin(spec)) {
    return false;
  }
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx)) {
    sk_draw_text(.x = ctx.x + 10, .y = ctx.y + 4, .text = ctx.text,
                 .align = "LEFT", .pad = ctx.x + 10);
  }
  sk_listbox_end();
  return true;
}

bool sk_checkbox_is_checked(const char *name) {
  bool r = Checkbox_IsChecked(name);
  return r;
}

bool sk_checkbox_is_hovered(const char *name) {
  bool r = Checkbox_IsHovered(name);
  return r;
}

bool ski_place_checkbox(const SkCheckboxSpec *spec) {
  bool r = Checkbox_Place(spec);
  return r;
}

SkWindowList sk_get_all_window_titles() {
  SkWindowList l = Platform_GetWindowTitles();
  return l;
}

SkWindowList sk_get_all_window_names() {
  SkWindowList l = Platform_GetWindowNames();
  return l;
}

PlColor sk_get_pixel_color(SkPos pos) {
  PlColor c = Platform_GetPixelColor(pos.x, pos.y);
  return c;
}
bool sk_move_to_pos(SkPos pos) {
  bool ok = Platform_MoveToPos(pos.x, pos.y);
  return ok;
}
bool sk_click_at_pos(SkPos pos) {
  bool ok = sk_move_to_pos(pos) && sk_click();
  return ok;
}
bool sk_left_click(void) {
  bool ok = Platform_ClickButton(1);
  return ok;
}
bool sk_right_click(void) {
  bool ok = Platform_ClickButton(2);
  return ok;
}
bool sk_emit_mouse_button(int button, bool down) {
  bool ok = Platform_MouseButtonDown(button, down);
  return ok;
}
bool sk_emit_mouse_button_at(SkPos pos, int button, bool down) {
  bool moved = sk_move_to_pos(pos);
  bool ok = moved && Platform_MouseButtonDown(button, down);
  return ok;
}
bool sk_click_button_at(SkPos pos, int button) {
  bool moved = sk_move_to_pos(pos);
  bool down = moved && Platform_MouseButtonDown(button, true);
  if (!down) {
    return false;
  }
  sk_sleep(0.02);
  bool up = Platform_MouseButtonDown(button, false);
  return up;
}
bool sk_mouse_scroll(int notches) {
  bool ok = Platform_MouseScroll(notches);
  return ok;
}

static bool sk_rel_to_abs(int rx, int ry, SkPos *out) {

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
  if (!sk_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = sk_move_to_pos(abs);
  return ok;
}

bool sk_click_at_pos_rel(int rx, int ry) {
  SkPos abs;
  if (!sk_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = sk_click_at_pos(abs);
  return ok;
}

bool sk_emit_mouse_button_at_rel(int rx, int ry, int button, bool down) {
  SkPos abs;
  if (!sk_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = sk_emit_mouse_button_at(abs, button, down);
  return ok;
}

bool sk_click_button_at_rel(int rx, int ry, int button) {
  SkPos abs;
  if (!sk_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = sk_click_button_at(abs, button);
  return ok;
}

void sk_emit_key(int vk, bool down) { Platform_EmitKey(vk, down ? 1 : 0); }

bool sk_replay_hotkey(int vk, int mods) {
  bool matched = hotkey_process(vk, mods);
  return matched;
}
static bool sk_type_text_common(const char *s) {
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
      Platform_WM_ProcessEvents(wm);
    sk_sleep_platform(0.005);
  }
  if (old) {
    Platform_CopyToClipboard(old);
    free(old);
  }
  return true;
}

bool sk_type_text(const char *s) { return sk_type_text_common(s); }

void sk_sleep(double seconds) { sk_sleep_platform(seconds); }

SkLibrary sk_library_open(const char *path) {
  return (SkLibrary)Platform_LoadLibrary(path);
}

void *sk_library_symbol(SkLibrary lib, const char *name) {
  return Platform_GetSymbol((LibHandle)lib, name);
}

void sk_library_close(SkLibrary lib) { Platform_UnloadLibrary((LibHandle)lib); }

const char *sk_library_error(void) { return Platform_GetLoadError(); }

typedef struct {
  SkThreadFunc fn;
  void *arg;
  char owner[512];
} SkThreadCtx;

static void *sk_thread_trampoline(void *p) {
  SkThreadCtx *ctx = (SkThreadCtx *)p;
  SkThreadFunc fn = ctx->fn;
  void *arg = ctx->arg;
  void *ret = fn ? fn(arg) : NULL;
  ThreadRegistry_Finish(ctx);
  free(ctx);
  return ret;
}

static SkThread sk_thread_create_ex(SkThreadFunc fn, void *arg,
                                    void *owner_src) {
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
  ThreadRegistry_Add(ctx, ctx->owner);
  void *thread = Platform_ThreadCreate(sk_thread_trampoline, ctx);
  if (!thread) {
    ThreadRegistry_Finish(ctx);
    free(ctx);
    return NULL;
  }
  return (SkThread)thread;
}

SkThread sk_thread_create(SkThreadFunc fn, void *arg) {
  return sk_thread_create_ex(fn, arg, NULL);
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

bool sk_hotkey_to_string(SkKey key, char *out, size_t cap) {
  if (!out || cap == 0)
    return false;
  if (!HotkeyString_Format(key, out, (int)cap)) {
    out[0] = '\0';
    return false;
  }
  return true;
}

SkKey sk_hotkey_from_string(const char *str) {
  SkKey key = {0};
  HotkeyString_Parse(str, &key);
  return key;
}

bool sk_is_key_down(int vk) { return Input_IsKeyDown(vk); }
bool sk_is_key_pressed(int vk) { return Input_WasKeyPressed(vk); }
bool sk_is_key_released(int vk) { return Input_WasKeyReleased(vk); }
bool sk_ctrl_pressed(void) { return Key_IsCtrlPressed(); }
bool sk_shift_pressed(void) { return Key_IsShiftPressed(); }
bool sk_alt_pressed(void) { return Key_IsAltPressed(); }
bool sk_super_pressed(void) { return Key_IsSuperPressed(); }
void sk_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud) {
  Input_SetKeyCallback(cb, ud);
}
int sk_get_key_pressed(void) { return Input_GetPressedKey(); }
char sk_get_char_pressed(void) { return Input_GetPressedChar(); }

bool sk_poll_key(SkKeyEvent *out) {
  if (!out)
    return false;
  PlatformKeyEvent ev;
  if (!GlobalKey_Pop(&ev))
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
  if (!GlobalMouse_Pop(&ev))
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

int sk_mouse_queue_len(void) { return GlobalMouse_Pending(); }

unsigned long sk_mouse_queue_dropped(void) { return GlobalMouse_Dropped(); }

void sk_clear_mouse_queue(void) { GlobalMouse_Clear(); }

SkPos sk_get_cursor_pos() {
  int x, y;
  Platform_GetMousePos(&x, &y);
  return (SkPos){x, y};
}

bool sk_mouse_pressed(int button) { return Input_MouseStateDown(button); }

int sk_key_queue_len(void) { return GlobalKey_Pending(); }

unsigned long sk_key_queue_dropped(void) { return GlobalKey_Dropped(); }

void sk_clear_key_queue(void) { GlobalKey_Clear(); }

int sk_char_for_key(int vk, int scancode, int mods) {
  return Platform_CharForKey(vk, scancode, mods);
}

bool sk_is_flag_prefix(char c) { return c == '-' || c == '/'; }

size_t sk_extract_flag_name(char *p, char *out, size_t cap) {
  size_t n = 0;
  while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '=')
    n++;
  if (n >= cap)
    n = cap - 1;
  memcpy(out, p, n);
  out[n] = '\0';
  return n;
}

bool sk_extract_quoted(char **pp, char *out, size_t cap) {
  char *p = *pp;
  char q = *p++;
  char *s = p;
  while (*p && *p != q)
    p++;
  size_t len = p - s;
  if (len >= cap)
    len = cap - 1;
  memcpy(out, s, len);
  out[len] = '\0';
  if (*p == q)
    p++;
  *pp = p;
  return true;
}

bool sk_extract_value(char **pp, char *out, size_t cap) {
  char *p = *pp;
  if (*p == '"' || *p == '\'')
    return sk_extract_quoted(pp, out, cap);
  char *s = p;
  while (*p && !isspace((unsigned char)*p))
    p++;
  size_t len = p - s;
  if (len >= cap)
    len = cap - 1;
  memcpy(out, s, len);
  out[len] = '\0';
  *pp = p;
  return len > 0;
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
    char name[64] = {0};
    size_t nlen = sk_extract_flag_name(p, name, sizeof(name));
    p += nlen;
    char valbuf[256] = {0};
    bool has_val = false;
    if (*p == '=') {
      p++;
      has_val = sk_extract_value(&p, valbuf, sizeof(valbuf));
    } else {
      char *save = p;
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p && !sk_is_flag_prefix(*p)) {
        has_val = sk_extract_value(&p, valbuf, sizeof(valbuf));
        if (has_val && sk_next_flag(p)) {
          has_val = false;
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
    if (has_val) {
      snprintf(f->value, sizeof(f->value), "%s", valbuf);
      f->has_value = true;
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
  Owner_ResolveForFunc((void *)callback, payload->owner,
                       sizeof(payload->owner));
  ThreadRegistry_Add(payload, payload->owner);
  if (!sk_run_after_time_arm_platform(payload)) {
    ThreadRegistry_Finish(payload);
    free(payload);
  }
}

SkTimer sk_start_timer(void) { return Platform_StartTimer(); }

double sk_end_timer(SkTimer t) { return Platform_EndTimer(t); }

char *sk_get_active_window_title(void) {
  char *title = Platform_GetActiveWindowTitle();
  if (!title || !title[0]) {
    return NULL;
  }
  char *r = strdup(title);
  return r;
}

char *sk_get_active_window_app_id(void) {
  char *id = Platform_GetActiveWindowAppId();
  if (!id || !id[0]) {
    return NULL;
  }
  char *r = strdup(id);
  return r;
}

bool sk_is_window_open(const char *title) {
  SkWindowList l = Platform_GetWindowTitles();
  bool found = false;
  for (int i = 0; i < l.count; i++) {
    if (String_ContainsCI(l.windows[i], title)) {
      found = true;
      break;
    }
  }
  sk_free_window_list(&l);
  return found;
}

char *sk_get_working_dir() { return Platform_GetWorkingDir(); }

char *sk_resolve_data_path(const char *name) {
  if (!name || !name[0])
    return NULL;
  char full[MAX_PATH];
  if (!FeaturePath_ResolveDataPath(__builtin_return_address(0), name, full,
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

SkTimer sk_profile_start() { return sk_start_timer(); }

void sk_profile_end(SkTimer timer) {
  double ms = sk_end_timer(timer);
  if (ms < 0.0)
    ms = 0.0;

  if (ms >= 1000.0) {
    printf("[profile] %.3f s\n", ms / 1000.0);
  } else if (ms >= 1.0) {
    printf("[profile] %.3f ms\n", ms);
  } else {
    printf("[profile] %.3f µs\n", ms * 1000.0);
  }
}

static SkWidget *sk_widgets[WIDGET_MAX];
static int sk_widget_count = 0;

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

static SkTimer mon_at;
static bool mon_valid = false;
static int mon_x, mon_y, mon_w, mon_h;

static void sk_widget_monitor_geometry(int *x, int *y, int *w, int *h) {

  if (!mon_valid || sk_end_timer(mon_at) > 2000) {
    Platform_WM_GetMonitorGeometry(&mon_x, &mon_y, &mon_w, &mon_h);
    mon_at = sk_start_timer();
    mon_valid = true;
  }
  *x = mon_x;
  *y = mon_y;
  *w = mon_w;
  *h = mon_h;
}

static void sk_widget_relayout(void);

static SkPos sk_hover_pos = {0, 0};
static SkTimer sk_hover_at = {0};
static bool sk_hover_valid = false;

static void sk_widget_hover_poll(void) {

  SkTimer now = sk_start_timer();
  if (sk_hover_valid && sk_end_timer(sk_hover_at) < 400.0) {
    return;
  }
  sk_hover_pos = sk_get_cursor_pos();
  sk_hover_at = now;
  sk_hover_valid = true;
}

static void *sk_widget_hover_worker(void *ud) {

  SkWidget *w = (SkWidget *)ud;
  if (w && w->on_hover)
    w->on_hover();
  if (w)
    w->hover_running = false;
  return NULL;
}

static void sk_widget_render_frame(void *ud) {

  SkWidget *widget = (SkWidget *)ud;
  if (!widget || !widget->render || widget->dead) {
    {
      return;
    }
  }
  widget->frame++;
  bool hoverable = (widget->on_hover != NULL);
  if (hoverable) {
    sk_widget_hover_poll();
    bool inside = (widget->x >= 0x7fffffff)
                      ? false
                      : (sk_hover_pos.x >= widget->x &&
                         sk_hover_pos.x < widget->x + widget->w &&
                         sk_hover_pos.y >= widget->y &&
                         sk_hover_pos.y < widget->y + widget->h);
    if (inside && !widget->hovered && widget->on_hover &&
        !widget->hover_running) {
      widget->hover_running = true;
      widget->hover_thread = sk_thread_create(sk_widget_hover_worker, widget);
      if (!widget->hover_thread)
        widget->hover_running = false;
    }
    widget->hovered = inside;
    if (!inside) {
      Platform_HoverMessageRelease(widget->name);
      HoverWidgetHide(widget->name);
    }
  }
  SkWidgetState st;
  st.width = widget->w;
  st.height = widget->h;
  st.frame = widget->frame;
  st.elapsed = sk_end_timer(widget->created_at);
  st.hovered = widget->hovered;

  SetDrawOffset(-widget->origin_x, 0);
  BeginMeasure();
  widget->render(&st);
  int mw = 0, mh = 0, mx = 0, my = 0;
  EndMeasure(&mw, &mh);
  GetLastMeasureOrigin(&mx, &my);
  mx += widget->origin_x;

  if (widget->origin_x != mx || widget->origin_y != my) {
    widget->origin_x = mx;
    widget->origin_y = my;
    widget_invalidate(widget->win);
  }

  if (widget->auto_w || widget->auto_h) {
    int nw = (widget->auto_w && mw > 0) ? mw + widget->pad : widget->w;
    int nh = (widget->auto_h && mh > 0) ? mh + widget->pad : widget->h;

    if (nw != widget->w || nh != widget->h) {
      widget->w = nw;
      widget->h = nh;
      if (widget_is_valid(widget->win)) {
        Platform_Widget_Resize(widget->win, nw, nh);
      }
      sk_widget_relayout();
    }
  }
  char prof_label[80];
  snprintf(prof_label, sizeof(prof_label), "skapi:widget[%s]", widget->name);
}

static void sk_widget_attach_window(void *ud) {
  SkWidget *widget = (SkWidget *)ud;
  if (!widget || widget->dead)
    return;
  bool created = false;
  if (!widget_is_valid(widget->win)) {
    SetDrawBgColor(widget->has_bg ? widget->bg_color : 0);
    widget->win = Platform_Widget_Create(widget->x, widget->y, widget->w,
                                         widget->h, widget->fps, widget->no_bg,
                                         widget->on_click, widget->name);
    SetDrawBgColor(0);
    created = (widget->win != NULL);
  } else {
    Platform_Widget_Move(widget->win, widget->x, widget->y);
  }
  if (widget->win) {
    Platform_Widget_SetBgColor(widget->win,
                               widget->has_bg ? widget->bg_color : 0);
    Platform_Widget_SetNoBg(widget->win, widget->no_bg);
    widget_set_fps(widget->win, widget->fps);
    if (created)
      widget_set_frame_callback(widget->win, sk_widget_render_frame, widget);
  }
}

static void sk_widget_apply_geometry(SkWidget *widget, int x, int y) {
  widget->x = x;
  widget->y = y;
  if (RenderThread_IsCurrentThread()) {

    sk_widget_attach_window(widget);
    return;
  }
  if (!RenderThread_EnsureStarted(widget->fps)) {
    return;
  }
  if (!RenderThread_Post(sk_widget_attach_window, widget)) {
    return;
  }
}

static int sk_widget_sort_key(const SkWidget *widget) {
  return widget->order_set ? widget->order : widget->seq;
}

static void sk_widget_relayout(void) {

  int mx = 0, my = 0, mw = 1920, mh = 1080;
  sk_widget_monitor_geometry(&mx, &my, &mw, &mh);

  SkWidget *sorted[WIDGET_MAX];
  int n = 0;
  for (int i = 0; i < sk_widget_count; i++) {
    if (sk_widgets[i] && !sk_widgets[i]->free_pos)
      sorted[n++] = sk_widgets[i];
  }
  for (int i = 1; i < n; i++) {
    SkWidget *key = sorted[i];
    int k = i - 1;
    while (k >= 0 && sk_widget_sort_key(sorted[k]) > sk_widget_sort_key(key)) {
      sorted[k + 1] = sorted[k];
      k--;
    }
    sorted[k + 1] = key;
  }

  for (int s = 0; s < 4; s++) {
    bool from_right =
        (s == WIDGET_SLOT_TOP_RIGHT || s == WIDGET_SLOT_BOTTOM_RIGHT);
    bool from_top = (s == WIDGET_SLOT_TOP_LEFT || s == WIDGET_SLOT_TOP_RIGHT);
    int cursor = from_right ? mx + mw : mx;
    for (int i = 0; i < n; i++) {
      SkWidget *w = sorted[i];
      if (w->slot != s)
        continue;
      int g = w->gap_override > 0 ? w->gap_override : WidgetGap;
      int x = from_right ? cursor - w->w : cursor;
      int y = from_top ? my : my + mh - w->h;
      sk_widget_apply_geometry(w, x, y);
      cursor += from_right ? -(w->w + g) : (w->w + g);
    }
  }
}

static bool monitor_name_matches(const char *a, const char *b) {
  if (!a || !b)
    return false;
  size_t la = strlen(a), lb = strlen(b);
  if (la == 0 || lb == 0)
    return false;
  for (size_t i = 0; i + lb <= la; i++)
    if (strncasecmp(a + i, b, lb) == 0)
      return true;
  return false;
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
      if (monitor_name_matches(info.name, copy.monitor_name)) {
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

SkPos sk_get_widget_pos(const char *name) { return widget_get_pos(name); }

void sk_widget_show(SkWidget *widget) { widget_show(widget); }

void sk_widget_hide(SkWidget *widget) { widget_hide(widget); }

void sk_widget_destroy(SkWidget *widget) { widget_destroy(widget); }

void sk_widget_set_render(SkWidget *widget, SkWidgetRenderFn render) {
  widget_set_render(widget, render);
}

void sk_format_bytes(unsigned long long bytes, char *buf, size_t buflen) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
  const int count = sizeof(units) / sizeof(units[0]);

  double value = (double)bytes;
  int i = 0;

  while (value >= 1024.0 && i < count - 1) {
    value /= 1024.0;
    i++;
  }

  if (i == 0) {
    snprintf(buf, buflen, "%llu %s", bytes, units[i]);
  } else {
    snprintf(buf, buflen, "%.1f %s", value, units[i]);
  }
}
SkNetStatus sk_net_status(void) { return sk_net_status_platform(); }

unsigned long long sk_net_rx_bytes() { return sk_net_rx_bytes_platform(); }

int sk_monitor_count(void) { return Monitor_Count(); }

bool sk_monitor_at(int index, SkMonitorInfo *out) {
  if (!out)
    return false;
  MonitorInfo info;
  if (!Monitor_Info(index, &info))
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

SkRect sk_get_monitor_geometry() {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return (PlRect){.x = x, .y = y, .w = w, .h = h, .color = 0};
}

int sk_get_screen_width() {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return w;
}

int sk_get_screen_height() {
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

static int cc_entry_count = 0;
static SkCcEntry cc_entries[4096];

#define SK_CC_DEP_OWNERS 256
static char cc_dep_owners[SK_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int cc_dep_owner_count = 0;

static bool sk_cc_dep_seen(const char *owner) {
  for (int i = 0; i < cc_dep_owner_count; i++)
    if (strcmp(cc_dep_owners[i], owner) == 0)
      return true;
  return false;
}

static void sk_cc_dep_mark(const char *owner) {
  if (cc_dep_owner_count >= SK_CC_DEP_OWNERS)
    return;
  snprintf(cc_dep_owners[cc_dep_owner_count++], OWNER_PATH_MAX, "%s", owner);
}

static const char *sk_cc_dup(char *dst, size_t cap, const char *src) {
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
      sk_cc_dup(e->display_name, sizeof(e->display_name), spec->display_name);
  char icon_asset[MAX_PATH];
  if (spec->icon && (spec->icon[0] == '/' || spec->icon[0] == '\\') &&
      FeaturePath_ResolveAssetPath(__builtin_return_address(0), spec->icon,
                                   icon_asset, sizeof(icon_asset))) {
    e->pub.icon = sk_cc_dup(e->icon, sizeof(e->icon), icon_asset);
  } else {
    e->pub.icon = sk_cc_dup(e->icon, sizeof(e->icon), spec->icon);
  }
  e->pub.type = sk_cc_dup(e->type, sizeof(e->type), spec->type);
  if (spec->internal_name) {
    e->pub.internal_name = sk_cc_dup(e->internal_name, sizeof(e->internal_name),
                                     spec->internal_name);
  } else {
    e->pub.internal_name = sk_cc_dup(e->internal_name, sizeof(e->internal_name),
                                     spec->display_name);
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
  Owner_ResolveForFunc(anchor, e->owner, sizeof(e->owner));
  if (e->owner[0] && !sk_cc_dep_seen(e->owner)) {
    sk_cc_dep_mark(e->owner);
    cc_add_dep("control_center.c", e->owner);
  }
  if (spec->type && strcmp(spec->type, "Feature") == 0)
    feature_list_add(spec->display_name, spec->on_action, spec->icon, e->owner);
  return true;
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
    if (Owner_ModuleMatches(cc_entries[i].owner, path_a, path_b))
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
static char g_alias_target[SK_MAX_ALIASES][256];
static int alias_count = 0;

bool ski_add_alias(const SkAliasSpec *spec) {
  if (!spec || !spec->alias || !spec->target)
    return false;
  if (alias_count >= SK_MAX_ALIASES)
    return false;
  for (int i = 0; i < alias_count; i++) {
    if (strcmp(g_alias_target[i], spec->target) == 0) {
      snprintf(alias_alias[i], sizeof(alias_alias[i]), "%s", spec->alias);
      return true;
    }
  }
  snprintf(alias_alias[alias_count], sizeof(alias_alias[alias_count]), "%s",
           spec->alias);
  snprintf(g_alias_target[alias_count], sizeof(g_alias_target[alias_count]),
           "%s", spec->target);
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
      snprintf(alias_alias[n], sizeof(alias_alias[n]), "%s", alias_alias[i]);
      snprintf(g_alias_target[n], sizeof(g_alias_target[n]), "%s",
               g_alias_target[i]);
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
    if (strcmp(g_alias_target[i], target) == 0)
      return alias_alias[i];
  }
  return NULL;
}

void ski_window(const SkPromptSpec *spec) {
  WindowManager_ShowPrompt(WindowManager_Get(), spec);
}

const char *sk_get_user_name(void) { return Platform_GetRealUser(); }

bool sk_is_root() { return Platform_IsRunningAsRoot(); }
void sk_reload_feature(char *name) { FeatureLoader_Reload(name); }

int sk_get_pid() { return sk_get_pid_platform(); }

void sk_reload_all_features(void) {
  if (!FeaturePath || !FeaturePath[0])
    return;
  DIR *dir = opendir(FeaturePath);
  if (!dir)
    return;
  const char *ext = Platform_LibExtension();
  struct dirent *ent;
  int reloaded = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.')
      continue;
    char subpath[512];
    snprintf(subpath, sizeof(subpath), "%s/%s", FeaturePath, ent->d_name);
    struct stat st;
    if (stat(subpath, &st) != 0 || !S_ISDIR(st.st_mode))
      continue;
    char src[512], dll[512];
    snprintf(src, sizeof(src), "%s/%s.c", subpath, ent->d_name);
    snprintf(dll, sizeof(dll), "build/features/%s%s", ent->d_name, ext);
    struct stat st_src, st_dll;
    bool should_build = false;
    if (stat(src, &st_src) == 0) {
      if (stat(dll, &st_dll) != 0 || st_src.st_mtime > st_dll.st_mtime) {
        should_build = true;
      }
    }
    if (!should_build)
      continue;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", dll, sk_get_pid());
    char cmd[4096];
    if (!Platform_GetFeatureBuildCommand(src, tmp, cmd, sizeof(cmd))) {
      remove(tmp);
      continue;
    }
    char *outStr = sk_run_terminal_command(cmd);
    bool ok = false;
    if (outStr) {
      bool failed = strstr(outStr, "error:") || strstr(outStr, "error C") ||
                    strstr(outStr, "error LNK") ||
                    strstr(outStr, "undefined reference") ||
                    strstr(outStr, "failed");
      ok = !failed;
      free(outStr);
    } else {
      ok = true;
    }
    if (!ok) {
      remove(tmp);
      continue;
    }
    bool reloaded_ok = FeatureLoader_ReloadFrom(ent->d_name, tmp);
    remove(tmp);
    if (!reloaded_ok)
      continue;
    struct utimbuf times;
    times.actime = st_src.st_mtime;
    times.modtime = st_src.st_mtime;
    utime(dll, &times);
    reloaded++;
  }
  closedir(dir);
  char msg[128];
  snprintf(msg, sizeof(msg), "Reloaded %d feature(s)!", reloaded);
  sk_message(.message = msg, .slot = BOTTOM_CENTER, .timeout = 1.0);
}

bool sk_hotkey_down(SkKey key) {
  if (!sk_is_key_down(key.vk_code))
    return false;
  if ((key.mods & SK_CTRL) && !sk_ctrl_pressed())
    return false;
  if ((key.mods & SK_SHIFT) && !sk_shift_pressed())
    return false;
  if ((key.mods & SK_ALT) && !sk_alt_pressed())
    return false;
  if ((key.mods & SK_SUPER) && !sk_super_pressed())
    return false;
  return true;
}

void sk_register_key_interceptor(SkKeyInterceptorFn fn) {
  Key_RegisterInterceptor((KeyInterceptorFn)fn);
}

typedef struct {
  double seconds;
  void (*callback)(void);
} SkRunEveryArgs;

static void *sk_run_every_thread(void *arg) {
  SkRunEveryArgs *args = (SkRunEveryArgs *)arg;
  while (true) {
    sk_sleep(args->seconds);
    args->callback();
  }
  free(args);
  return NULL;
}

void sk_run_every(double seconds, void (*callback)(void)) {
  SkRunEveryArgs *args = malloc(sizeof(SkRunEveryArgs));
  args->seconds = seconds;
  args->callback = callback;
  sk_thread_create_ex(sk_run_every_thread, args, (void *)(uintptr_t)callback);
}

SkMetrics *sk_get_metrics(void) { return Metrics_Get(); }
void *sk_get_active_prompt(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (top && top->user_data && top->type == POPUP_PROMPT) {
    return top->user_data;
  }
  return NULL;
}

void sk_register_event(SkEvent event, SkEventCallback cb) {
  EventBus_Register(event, cb);
}

void sk_unregister_event(SkEvent event, SkEventCallback cb) {
  EventBus_Unregister(event, cb);
}
void sk_close_window() { SkaldContext_Get()->window.selection_complete = true; }
