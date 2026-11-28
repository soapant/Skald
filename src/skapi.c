#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
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
#include "core/feature_store.h"
#include "core/editbox.h"
#include "core/focus.h"
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
#include "helpers/helpers_monitor.h"
#include "core/window_manager.h"
#include "core/prompt_keys.h"
#include "core/theme.h"
#include "core/thread_registry.h"
#include "core/http.h"
#include "core/owner_purge.h"
#include "core/scroll_area.h"
#include "core/lua_host.h"

#include "platform/lib.h"
#include "core/clipboard_history.h"
#include "core/skapi.h"
#include "core/listbox_hover.h"

#include "core/config_section.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "core/feature_loader.h"
#include "core/feature_path.h"
#include "core/reload_lib.h"
#include "core/cap_registry.h"
#include "core/owner_registry.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_misc.h"

#include "platform/widget.h"
#include "core/hover_widget.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/hotkey_capture.h"
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

void *sk_array_grow(void *arr, size_t elem_size, size_t add, size_t min_cap) {
  return stbds_arrgrowf(arr, elem_size, add, min_cap);
}

size_t sk_array_len(const void *arr) {
  return arr ? stbds_header((char **)arr)->length : 0;
}

void *sk_array_set_len(void *arr, size_t elem_size, size_t len) {
  size_t old = sk_array_len(arr);
  if (len > old)
    arr = stbds_arrgrowf(arr, elem_size, len - old, 0);
  if (arr)
    stbds_header((char **)arr)->length = len;
  return arr;
}

void *sk_array_push(void *arr, size_t elem_size, const void *item) {
  size_t len = sk_array_len(arr);
  arr = stbds_arrgrowf(arr, elem_size, 1, 0);
  if (!arr)
    return NULL;
  stbds_header((char **)arr)->length = len + 1;
  char *slot = (char *)arr + len * elem_size;
  if (item)
    memcpy(slot, item, elem_size);
  else
    memset(slot, 0, elem_size);
  return arr;
}

void sk_array_free(void *arr) {
  if (arr)
    stbds_arrfreef(arr);
}

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

bool ski_add_dep(const char *dep_name, const char *owner_file) {
  return cc_add_dep(dep_name, owner_file);
}

unsigned sk_cap_generation(void) { return cap_generation(); }

void ski_cap_register(const char *key, void *fn) {
  cap_register_abi(key, CAP_ABI_V1, fn);
}

void *sk_cap_resolve(const char *key) {
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
  return load_file_list_from(__builtin_return_address(0), filepath);
}

char *sk_load_file(const char *filepath) {
  if (!filepath)
    return NULL;
  char full[MAX_PATH];
  if (!resolve_caller_path(__builtin_return_address(0), filepath, full,
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
  if (!resolve_caller_path_silent(__builtin_return_address(0), filepath, full,
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
  return write_lines_from(__builtin_return_address(0), filepath, lines, count);
}

ColorRef sk_rgb(unsigned r, unsigned g, unsigned b) { return RGB(r, g, b); }

ColorRef sk_hex(unsigned long h) { return HEX(h); }

const char *sk_platform(void) {
#if defined(_WIN32)
  return "win32";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__ANDROID__)
  return "android";
#else
  return "linux";
#endif
}

const char *ski_store_feature_name(const char *source_file) {
  static _Thread_local char name[128];
  if (!feature_store_feature_name(source_file, name, sizeof(name)))
    name[0] = '\0';
  return name;
}

bool ski_store_set(const char *feature, const char *key, const char *value) {
  return feature_store_set(feature, key, value);
}

bool ski_store_append(const char *feature, const char *key, const char *value) {
  return feature_store_append(feature, key, value);
}

SkStringList ski_store_get_all(const char *feature, const char *key) {
  return feature_store_get_all(feature, key);
}

char *ski_store_get_last(const char *feature, const char *key) {
  return feature_store_get_last(feature, key);
}

bool ski_store_replace_all(const char *feature, const char *key,
                           const char *const *values, int count) {
  return feature_store_replace_all(feature, key, values, count);
}

bool ski_store_clear(const char *feature, const char *key) {
  return feature_store_clear(feature, key);
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
  char *buf = file_read_all_alloc(expanded);
  if (!buf)
    return NULL;
  if (out_len)
    *out_len = strlen(buf);
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

bool sk_remove_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  return filter_lines_from(__builtin_return_address(0), path, line, false);
}

bool sk_remove_line_contains(const char *path, const char *substr) {
  if (!path || !substr)
    return false;
  return filter_lines_from(__builtin_return_address(0), path, substr, true);
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

bool ski_register_remapping(const SkRemappingSpec *spec) {
  if (!spec || !spec->src || !spec->dst)
    return false;
  const char *const *wl = spec->whitelist;
  const char *const *bl = spec->blacklist;
  const char *const *windows;
  bool whitelist;
  resolve_window_list(wl, bl, &windows, &whitelist);
  return remap_register_internal(spec->src, spec->dst, whitelist, windows,
                                 (void *)__builtin_return_address(0));
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

bool sk_unregister_snippet(const char *trigger) {
  return snippet_unregister_internal(trigger);
}

SkFeatureList sk_get_feature_list(void) {
  SkFeatureList list = {NULL, 0};
  int count = feature_list_count();
  if (count <= 0)
    return list;
  SkFeature *items = (SkFeature *)malloc((size_t)count * sizeof(*items));
  if (!items)
    return list;
  for (int i = 0; i < count; i++) {
    feature_list_at(i, &items[i].name, &items[i].callback, &items[i].icon);
  }
  list.items = items;
  list.count = (size_t)count;
  return list;
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
  if (!binding.valid)
    return false;
  binding.hotkey_callback_ud = spec->cb_ud;
  binding.hold = false;
  binding.should_swallow = spec->should_swallow;
  return hotkey_register_internal(binding);
}

bool sk_unregister_hotkey(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyBinding binding = hotkey_convert_into_binding((char *)hotkey, NULL,
                                                      NULL, NULL, false, NULL);
  if (!binding.valid)
    return false;
  return hotkey_unregister_internal(binding.vk, binding.modifiers);
}

void sk_disable_global_hotkeys(void) { hotkey_set_suspended(true); }

void sk_enable_global_hotkeys(void) { hotkey_set_suspended(false); }

void sk_hotkey_capture_begin(void) { hotkey_capture_begin(); }

bool sk_hotkey_capture_poll(char *out, size_t cap) {
  return hotkey_capture_poll(out, cap);
}

const char *sk_hotkey_capture_preview(void) { return hotkey_capture_preview(); }

const char *sk_hotkey_capture_last(void) { return hotkey_capture_last(); }

bool sk_hotkey_capture_active(void) { return hotkey_capture_active(); }

void sk_hotkey_capture_end(void) { hotkey_capture_end(); }

int sk_hotkey_count(void) { return hotkey_count(); }

char *sk_hotkey_string_at(int index) {
  return hotkey_binding_format(hotkey_get(index));
}

char *sk_hotkey_for_ud(void *ud) {
  return hotkey_binding_format(hotkey_find_by_ud(ud));
}

void sk_register_deferred_init(void (*cb)(void)) { deferred_init_register(cb); }

char *sk_hotkey_for_callback(void (*cb)(void)) {
  return hotkey_binding_format(hotkey_find_by_callback((void (*)(void))cb));
}

bool sk_hotkey_taken(const char *hotkey) {
  if (!hotkey)
    return false;
  HotkeyKey key = {0};
  if (!hotkey_string_parse(hotkey, &key))
    return false;
  return hotkey_is_taken(key.vk, key.mods);
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
  return String_Split(str, delimiter);
}

void sk_string_list_free(StringList *list) { string_list_free(list); }

void ski_populate_listbox(const SkPopulateListboxSpec *spec) {
  if (!spec)
    return;
  if (search_index_current())
    return;
  populate_listbox_ranked(spec->lb, spec->filter, spec->items, spec->count,
                          spec->filter_match, spec->item_rank);
}

SkSearchIndex *sk_search_create(void) { return search_index_create(); }

void sk_search_destroy(SkSearchIndex *idx) { search_index_destroy(idx); }

void sk_search_push_context(SkSearchIndex *idx, int source_id,
                            const char *context) {
  search_index_push_context(idx, source_id, context);
}

void sk_search_pop_context(SkSearchIndex *idx) {
  search_index_pop_context(idx);
}

void sk_search_add_text(SkSearchIndex *idx, const char *text, SkSearchKind kind,
                        const char *widget_name, const char *value) {
  search_index_add_text(idx, text, kind, widget_name, value);
}

void sk_search_set_activate(SkSearchIndex *idx, SkSearchActivateFn activate,
                            void *ud) {
  search_index_set_activate(idx, activate, ud);
}

void sk_search_reindex(SkSearchIndex *idx, SkSearchRenderFn render, void *ud) {
  search_index_reindex(idx, render, ud);
}

int sk_search_query(SkSearchIndex *idx, const char *filter) {
  return search_index_query(idx, filter);
}

int sk_search_hit_count(const SkSearchIndex *idx) {
  return search_index_hit_count(idx);
}

const SkSearchHit *sk_search_hit_at(const SkSearchIndex *idx, int i) {
  return search_index_hit_at(idx, i);
}

void sk_search_activate(SkSearchIndex *idx, int hit_i) {
  search_index_activate(idx, hit_i);
}

bool sk_is_selected(ListBox *lb) {
  if (!lb)
    return false;
  return lb->selected_index != -1;
}

void sk_set_edit_text(const char *text) {
  PromptState *state = current_prompt_state();
  if (!state || !state->eb)
    return;
  safe_copy(state->eb->filter, sizeof(state->eb->filter), text);
  state->eb->cursor = (size_t)strlen(state->eb->filter);
}

void sk_set_edit_cursor(int position) {
  PromptState *state = current_prompt_state();
  if (!state || !state->eb)
    return;
  size_t len = strlen(state->eb->filter);
  size_t cursor = position < 0 ? 0 : (size_t)position;
  state->eb->cursor = cursor > len ? len : cursor;
}

int sk_get_edit_cursor(void) {
  PromptState *state = current_prompt_state();
  return state && state->eb ? (int)state->eb->cursor : 0;
}

char *sk_get_edit_text(void) {
  PromptState *state = current_prompt_state();
  return state && state->eb ? strdup(state->eb->filter) : strdup("");
}

void ski_message(const SkMessageSpec *spec) {
  SkaldContext *ctx = SkaldContext_Get();
  if ((ctx && ctx->window.headless.active) || !MessagesEnabled)
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
      HoverPopupCtx *popup = hover_popup_ctx_new(
          &(HoverPopupCreateSpec){.render = spec->render,
                                  .render_data = spec->render_data,
                                  .w = spec->w,
                                  .h = spec->h});
      if (popup) {
        hover_widget_show((HoverWidgetShowSpec){
            .key = spec->hover_key,
            .render = hover_popup_frame,
            .ud = popup,
            .owner_src = (void *)spec->render,
            .x = spec->x,
            .y = spec->y,
            .w = popup->w,
            .h = popup->h,
            .fps = spec->fps > 0 ? spec->fps : 1,
            .no_bg = spec->no_bg,
            .bg_hex = spec->color_bg
                          ? rgb_to_hex(spec->color_bg)
                          : (spec->no_bg ? 0UL : rgb_to_hex(COLOR_BG)),
            .release = hover_popup_ctx_free,
            .release_ud = popup});
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

char *ski_edit_text_popup(const SkEditPopupSpec *spec) {
  if (!spec)
    return NULL;
  const char *initial_text = spec->initial_text;
  int x = spec->x ? spec->x : 0;
  int y = spec->y ? spec->y : 0;
  int w = spec->w ? spec->w : 600;
  int h = spec->h ? spec->h : 40;
  ListBox *lb = spec ? spec->lb : NULL;
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
  TextPromptRenderData render_data = {0};
  PromptSpec pspec =
      text_prompt_spec(&render_data, NULL, initial_text, x, y, w, h);
  {
    SkaldContext *ctx = SkaldContext_Get();
    if (ctx) {
      ctx->window.editbox_placed = false;
      ctx->window.listbox_placed = false;
      ctx->window.band_active = false;
    }
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

SkWindowHandle sk_find_window_by_title(const char *title) {
  return Platform_GetWindowByTitle(title);
}

SkWindowHandle sk_find_window_by_root_atom(const char *atom) {
  return Platform_GetWindowByRootAtom(atom);
}

SkWindowHandle sk_find_window_by_id(const char *id) {
  return Platform_GetWindowById(id);
}

SkWindowHandle sk_get_held_window(void) { return Platform_GetHeldWindow(); }

bool sk_move_window(SkWindowHandle window, SkPos pos) {
  return Platform_MoveWindow(window, pos.x, pos.y);
}

bool sk_set_window_size(SkWindowHandle window, int w, int h) {
  return Platform_SetWindowSize(window, w, h);
}

bool sk_set_window_opacity(SkWindowHandle window, int percent) {
  return Platform_SetWindowOpacityFor(window, percent);
}

SkRect sk_get_window_geometry(SkWindowHandle window) {
  return Platform_GetWindowGeometry(window);
}

bool ski_set_window_geometry(SkWindowHandle window, const SkRect *rect) {
  if (!rect)
    return false;
  return Platform_SetWindowGeometry(window, *rect);
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

bool sk_save_config(void) { return config_save(); }

void sk_refresh_ui(void) {
  Platform_ResetUIScale();
  Platform_ApplyUIScale();
  Platform_PrewarmBackgroundPixmap();
  UI_RequestPopupRepaint();
}

void sk_request_refresh(void) {
  prompt_refresh(current_prompt_state());
  UI_RequestPopupRepaint();
}

bool sk_commit_config(void) {
  bool ok = sk_save_config();
  sk_refresh_ui();
  return ok;
}

const char *sk_get_theme(void) {
  const char *theme = atomic_load_explicit(&Theme, memory_order_acquire);
  return (theme && theme[0]) ? theme : NULL;
}

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
  atomic_store_explicit(&Theme, e->name, memory_order_release);
  theme_apply();
  if (!config_save())
    log_warn("could not save theme selection");
  UI_RequestPopupRepaint();
  return true;
}

char *ski_popup(const PopupSpec *spec) {
  if (!spec)
    return NULL;
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  const char *placeholder = spec->placeholder;
  if (spec->password_mode && (!placeholder || !placeholder[0]))
    placeholder = "Enter Password:";
  TextPromptRenderData render_data = {0};
  PromptSpec pspec =
      text_prompt_spec(&render_data, placeholder, NULL, spec->x, spec->y,
                       spec->w ? spec->w : 600, spec->h ? spec->h : 70);
  pspec.password_mode = spec->password_mode;
  char *r = WindowManager_ShowPrompt(wm, &pspec);
  return r ? strdup(r) : NULL;
}

bool ski_draw_text(const SkTextSpec *spec) {
  if (!spec) {
    return false;
  }
  char tmp[2] = {0};
  const char *real_text = text_resolve(spec->text, tmp);
  if (!real_text)
    return false;
  if (capture_text(real_text))
    return true;
  unsigned long col = draw_color(spec->color);
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
                                                .font_size = spec->font_size,
                                                .angle = spec->angle});
}

bool ski_draw_text_segments(const SkTextSegmentsSpec *spec) {
  if (!spec || !spec->segments || spec->count <= 0)
    return false;
  int total = 0;
  for (int i = 0; i < spec->count; i++)
    total += text_segment_width(&spec->segments[i], spec->font_size);
  int start_x = spec->pad > 0 ? spec->pad : spec->x;
  const char *align = spec->align ? spec->align : "LEFT";
  if (string_contains_ci(align, "CENTER"))
    start_x -= total / 2;
  else if (string_contains_ci(align, "RIGHT"))
    start_x -= total;
  int cx = start_x;
  bool ok = true;
  for (int i = 0; i < spec->count; i++) {
    const SkTextSegment *seg = &spec->segments[i];
    if (!seg->text || !seg->text[0])
      continue;
    unsigned long color = seg->color;
    if (color == 0 || color == COLOR_AUTO)
      color = spec->color;
    if (!ski_draw_text(&(SkTextSpec){.text = seg->text,
                                     .x = cx,
                                     .y = spec->y,
                                     .color = color,
                                     .font = spec->font,
                                     .font_size = spec->font_size,
                                     .align = "LEFT",
                                     .no_hscroll = spec->no_hscroll,
                                     .angle = spec->angle}))
      ok = false;
    cx += text_segment_width(seg, spec->font_size);
  }
  return ok;
}

int ski_draw_text_wrapped(const SkTextWrappedSpec *spec) {
  if (!spec)
    return 0;
  int fs = spec->font_size > 0 ? spec->font_size : FontSize;
  int line_h = spec->line_height > 0 ? spec->line_height : fs + 4;
  int max_w = spec->max_width > 40 ? spec->max_width : 40;
  if (search_index_current() && !spec->measure_only) {
    if (spec->segments && spec->count > 0) {
      for (int i = 0; i < spec->count; i++) {
        const char *text = spec->segments[i].text;
        if (text && text[0])
          search_index_add_text(NULL, text, SK_SEARCH_TEXT, NULL, NULL);
      }
    } else if (spec->text && spec->text[0]) {
      search_index_add_text(NULL, spec->text, SK_SEARCH_TEXT, NULL, NULL);
    }
    return spec->y;
  }
  bool use_segments = spec->segments && spec->count > 0;
  int nseg = use_segments ? spec->count : (spec->text ? 1 : 0);
  if (!nseg)
    return spec->y;
  WrappedWord *words = NULL;
  int count = 0, cap = 0;
  int space_w = sk_get_text_width(" ", fs);
  int pending_breaks = 0;
  int lead = 0;
  for (int si = 0; si < nseg; si++) {
    const char *text = use_segments ? spec->segments[si].text : spec->text;
    unsigned long color = use_segments ? spec->segments[si].color : spec->color;
    if (!text)
      continue;
    char word[256];
    size_t wn = 0;
    for (const char *p = text; *p; p++) {
      unsigned char ch = (unsigned char)*p;
      if (ch == '\n') {
        if (wn) {
          wrapped_word_push(&words, &count, &cap, word, wn, color, lead,
                            pending_breaks, fs);
          wn = 0;
          pending_breaks = 0;
        }
        pending_breaks++;
        lead = 0;
        continue;
      }
      if (isspace(ch)) {
        if (wn) {
          wrapped_word_push(&words, &count, &cap, word, wn, color, lead,
                            pending_breaks, fs);
          wn = 0;
          pending_breaks = 0;
          lead = 0;
        }
        lead++;
        continue;
      }
      if (wn < sizeof(word) - 1)
        word[wn++] = (char)ch;
    }
    if (wn)
      wrapped_word_push(&words, &count, &cap, word, wn, color, lead,
                        pending_breaks, fs);
  }
  int line = 0, cx = 0, max_line = 0;
  for (int i = 0; i < count; i++) {
    WrappedWord *w = &words[i];
    if (w->breaks) {
      line += w->breaks;
      cx = 0;
    }
    cx += w->lead * space_w;
    if (cx > 0 && cx + w->width > max_w) {
      line++;
      cx = w->lead * space_w;
    }
    w->line = line;
    w->x = cx;
    cx += w->width;
    if (line > max_line)
      max_line = line;
  }
  max_line += pending_breaks;
  int *line_w = calloc((size_t)max_line + 1, sizeof(*line_w));
  if (!line_w) {
    free(words);
    return spec->y;
  }
  for (int i = 0; i < count; i++) {
    WrappedWord *w = &words[i];
    int end = w->x + w->width;
    if (line_w[w->line] < end)
      line_w[w->line] = end;
  }
  if (!spec->measure_only) {
    const char *align = spec->align ? spec->align : "LEFT";
    for (int i = 0; i < count; i++) {
      WrappedWord *w = &words[i];
      int offset = 0;
      if (string_contains_ci(align, "CENTER"))
        offset = (max_w - line_w[w->line]) / 2;
      else if (string_contains_ci(align, "RIGHT"))
        offset = max_w - line_w[w->line];
      if (offset < 0)
        offset = 0;
      ski_draw_text(&(SkTextSpec){.text = w->text,
                                  .x = spec->x + offset + w->x,
                                  .y = spec->y + w->line * line_h,
                                  .color = w->color,
                                  .font = spec->font,
                                  .font_size = fs,
                                  .align = "LEFT",
                                  .no_hscroll = spec->no_hscroll,
                                  .angle = spec->angle});
    }
  }
  free(line_w);
  free(words);
  return spec->y + (max_line + 1) * line_h;
}

bool ski_draw_text_centered(const SkTextCenteredSpec *spec) {
  if (!spec)
    return false;
  char tmp[2] = {0};
  const char *real_text = text_resolve(spec->text, tmp);
  if (!real_text)
    return false;
  if (capture_text(real_text))
    return true;
  unsigned long col = draw_color(spec->color);
  Platform_DrawTextCentered(
      (DrawTextCenteredSpec){.x = spec->x,
                             .y = spec->y,
                             .w = spec->w,
                             .h = spec->h,
                             .text = real_text,
                             .color = col,
                             .font = spec->font,
                             .font_size = spec->font_size});
  return true;
}

int ski_text_wrap_layout(const char *text, SkPos pos, int max_width,
                         int line_height, SkWrapMeasureFn measure, void *ctx,
                         SkPos *out, int maxout) {
  return text_wrap_layout(text, pos.x, pos.y, max_width, line_height, measure,
                          ctx, out, maxout);
}

int ski_measure_text_wrapped(const SkMeasureSpec *spec) {
  if (!spec || !spec->text) {
    return 0;
  }
  int font_size = spec->font_size;
  if (font_size <= 0)
    font_size = FontSize;
  int line_height = spec->line_height;
  if (line_height <= 0)
    line_height = font_size + 4;
  WrapCtx ctx;
  ctx.color = 0;
  ctx.font_size = font_size;
  int cap = (int)strlen(spec->text) + 1;
  return text_wrap_layout(spec->text, spec->x, spec->y, spec->max_width,
                          line_height, wrap_measure_thunk, &ctx,
                          spec->out_positions, cap);
}

bool ski_place_editbox(const SkEditSpec *spec) {
  if (!spec)
    return false;
  EditSpec copy = {
      .name = spec->name,
      .buffer = spec->buffer,
      .buffer_size = spec->buffer_size,
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .placeholder = spec->placeholder,
      .initial_text = spec->initial_text,
      .color_bg = spec->color_bg,
      .color_text = spec->color_text,
      .font = spec->font,
      .font_size = spec->font_size,
      .caret_h = spec->caret_h,
      .caret_w = spec->caret_w,
      .caret_x = spec->caret_x,
      .caret_y = spec->caret_y,
      .caret_pos = spec->caret_pos,
      .text_y = spec->text_y,
      .auto_focus = spec->auto_focus,
      .hide_caret = spec->hide_caret,
      .primary = spec->primary,
      .no_tab = spec->no_tab,
      .wrapped = spec->wrapped,
  };
  return place_editbox(&copy);
}

const char *sk_editbox_text(const char *name) {
  EditBox *eb = editbox_registry_lookup(current_prompt_state(), name);
  return eb ? eb->filter : NULL;
}

bool sk_editbox_set_text(const char *name, const char *text) {
  if (!text)
    return false;
  PromptState *state = current_prompt_state();
  EditBox *eb = editbox_registry_lookup(state, name);
  if (!eb)
    return false;
  safe_copy(eb->filter, sizeof(eb->filter), text);
  eb->cursor = strlen(eb->filter);
  eb->sel_start = -1;
  eb->sel_end = -1;
  eb->scroll_offset = 0;
  editbox_registry_sync(state);
  return true;
}

const char *sk_editbox_active(void) {
  return editbox_registry_active_name(current_prompt_state());
}

void sk_editbox_set_active(const char *name) {
  editbox_registry_set_active(current_prompt_state(), name);
}

SkImage sk_image_from_window_title(const char *title) {
  return image_take(Platform_CaptureWindowByTitle(title));
}

SkImage sk_image_from_window_id(const char *id) {
  return image_take(Platform_CaptureWindowById(id));
}

void sk_image_free(SkImage image) { Platform_FreeImage(&image); }

bool ski_draw_image(const SkImageSpec *spec) {
  if (!spec)
    return false;
  if (search_index_current())
    return true;
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  if (spec->image.pixels && spec->image.w > 0 && spec->image.h > 0)
    return Platform_DrawImageBuffer(&spec->image, spec->x, spec->y, w, h);
  if (!spec->path || !spec->path[0])
    return false;
  char asset[MAX_PATH];
  const char *path = feature_path_resolve_asset_or_path(
      __builtin_return_address(0), spec->path, asset, sizeof(asset));
  return Platform_DrawImageAt(path, spec->x, spec->y, w, h);
}

SkSize ski_get_image_size(const char *path, int *w, int *h) {
  SkSize size = {0};
  if (w)
    *w = 0;
  if (h)
    *h = 0;
  if (!path || !path[0])
    return size;
  int pw = 0, ph = 0;
  size_t path_len = strlen(path);
  bool is_svg = path_len >= 4;
  if (is_svg) {
    const unsigned char *tail = (const unsigned char *)path + path_len - 4;
    is_svg = (tail[0] | 32) == '.' && (tail[1] | 32) == 's' &&
             (tail[2] | 32) == 'v' && (tail[3] | 32) == 'g';
  }
  if ((!is_svg || !svg_file_size(path, &pw, &ph)) &&
      !Platform_GetImageSize(path, &pw, &ph))
    return size;
  size.w = pw;
  size.h = ph;
  if (w)
    *w = pw;
  if (h)
    *h = ph;
  return size;
}

bool ski_draw_rect(const DrawRect *spec) {
  if (!spec)
    return false;
  if (search_index_current())
    return true;
  bool transparent = spec->transparent;
  bool ok = draw_rect_raw(spec, draw_color(spec->color), spec->thickness,
                          spec->filled, transparent);
  if (!spec->border)
    return ok;
  draw_rect_raw(spec, rgb_to_hex(COLOR_BORDER),
                spec->thickness > 0.0f ? spec->thickness : 1.0f, false,
                transparent);
  return ok;
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
  SkRect r = {
      .x = 0, .y = 0, .w = 0, .h = 0, .color = 0, .border = EnableBorders};
  if (!spec || !spec->text || !spec->text[0])
    return r;
  int fs = spec->font_size > 0 ? spec->font_size : FontSize;
  const char *a = spec->align ? spec->align : "CENTER";
  int w = Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = spec->text, .len = (int)strlen(spec->text), .font_size = fs});
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

int sk_get_text_width(const char *text, int font_size) {
  if (!text)
    return 0;
  return Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = text, .len = (int)strlen(text), .font_size = font_size});
}

int sk_get_text_height(int font_size) {
  return Platform_GetTextLineHeight(font_size);
}

ColorRef sk_color_lerp(ColorRef a, ColorRef b, float t) {
  return color_lerp_bgr(a, b, t);
}

void sk_set_draw_offset(SkPos pos) { set_draw_offset(pos.x, pos.y); }

SkPos sk_get_draw_offset(void) {
  int x = 0, y = 0;
  get_draw_offset(&x, &y);
  return SK_POS(x, y);
}

bool ski_scroll_begin(const SkScrollSpec *spec) {
  if (!spec)
    return false;
  char *owner =
      owner_resolve_for_func_alloc((void *)__builtin_return_address(0));
  ScrollAreaSpec inner = {
      .name = spec->name,
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .content_w = spec->content_w,
      .content_h = spec->content_h,
      .off_x = spec->off_x,
      .off_y = spec->off_y,
      .wheel_px = spec->wheel_px,
      .owner = owner,
  };
  bool ok = scroll_area_begin(&inner);
  owner_strfree(&owner);
  return ok;
}

void sk_scroll_end(void) { scroll_area_end(); }

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
  if (search_index_current())
    return true;
  return Platform_DrawLineRaw(
      (DrawLineRawSpec){.x1 = spec->x1,
                        .y1 = spec->y1,
                        .x2 = spec->x2,
                        .y2 = spec->y2,
                        .color = draw_color(spec->color),
                        .thickness = spec->thickness});
}

bool ski_draw_circle(const Circle *spec) {
  if (!spec) {
    return false;
  }
  if (search_index_current())
    return true;
  return Platform_DrawCircleRaw(
      (DrawCircleRawSpec){.cx = spec->x,
                          .cy = spec->y,
                          .radius = spec->radius,
                          .color = draw_color(spec->color),
                          .thickness = spec->thickness});
}

void sk_listbox_end(void) { listbox_common_end(); }

bool ski_listbox_begin(const SkListSpec *spec) {
  if (!spec)
    return false;
  if (search_index_current()) {
    for (int i = 0; i < spec->count && spec->items; i++)
      search_index_add_text(NULL, spec->items[i], SK_SEARCH_TEXT, NULL, NULL);
    return false;
  }
  return listbox_begin_common(spec);
}

bool sk_listbox_next(SkListItemCtx *out) {
  if (!out)
    return false;
  return listbox_next_common((ListItemCtx *)out);
}

bool ski_place_listbox(const SkListSpec *spec) {
  if (!spec) {
    return false;
  }
  if (!ski_listbox_begin(spec)) {
    return false;
  }
  int line_h = Platform_GetTextLineHeight(0);
  int pad_top = (listbox_common_item_height() - line_h) / 2;
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
  return checkbox_take_clicked(name, PLACE_BUTTON);
}

bool ski_place_checkbox(const SkCheckboxSpec *spec) {
  if (!spec)
    return false;
  if (search_index_current()) {
    SkSearchKind kind = spec->button ? SK_SEARCH_BUTTON : SK_SEARCH_CHECKBOX;
    const char *text = (spec->text && spec->text[0]) ? spec->text : spec->name;
    search_index_add_text(NULL, text, kind, spec->name, NULL);
    return true;
  }
  if (spec->name && settings_focus_ctx_has(spec->name) &&
      spec->on_click == settings_dispatch_widget_click) {
    SkCheckboxSpec copy = *spec;
    copy.on_click = settings_dispatch_focus_click;
    return checkbox_place(&copy);
  }
  return checkbox_place(spec);
}

bool ski_place_slider(const SkSliderSpec *spec) {
  if (!spec)
    return false;
  if (search_index_current()) {
    char value[32];
    snprintf(value, sizeof(value), "%g", spec->value);
    search_index_add_text(NULL, spec->name, SK_SEARCH_SLIDER, spec->name,
                          value);
    return true;
  }
  if (spec->name && settings_focus_ctx_has(spec->name) &&
      spec->on_change == settings_dispatch_slider_change) {
    SkSliderSpec copy = *spec;
    copy.on_change = settings_dispatch_focus_change;
    return slider_place(&copy);
  }
  return slider_place(spec);
}

SkFocusKind sk_focus_kind(void) { return (SkFocusKind)focus_kind(); }

bool sk_focus_is(const char *name) { return focus_is(name); }

bool sk_focus_has_entries(void) { return focus_has_entries(); }

const char *sk_focus_name(void) { return focus_name(); }

bool sk_focus_move_next(void) { return focus_move(1); }

bool sk_focus_move_before(void) { return focus_move(-1); }

bool sk_focus_move_dir(SkFocusDir dir) {
  return focus_move_direction((FocusDirection)dir);
}

bool sk_activate_focused(void) { return focus_activate(); }

bool sk_focus_adjust(int dir) { return focus_adjust(dir); }

bool sk_set_focus(SkFocusKind kind) { return focus_set_kind((FocusKind)kind); }

void sk_focus_clear(void) { focus_clear(); }

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

SkColor sk_get_pixel_color(SkPos pos) {
  return Platform_GetPixelColor(pos.x, pos.y);
}

bool sk_move_to_pos(SkPos pos) { return Platform_MoveToPos(pos.x, pos.y); }

bool sk_click_at(SkPos pos) { return sk_move_to_pos(pos) && sk_click(); }

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

bool sk_move_to_pos_rel(SkPos pos) {
  SkPos abs;
  if (!rel_to_abs(pos, &abs)) {
    return false;
  }
  return sk_move_to_pos(abs);
}

bool sk_click_at_rel(SkPos pos) {
  SkPos abs;
  if (!rel_to_abs(pos, &abs)) {
    return false;
  }
  return sk_click_at(abs);
}

bool sk_emit_mouse_button_at_rel(SkPos pos, int button, bool down) {
  SkPos abs;
  if (!rel_to_abs(pos, &abs)) {
    return false;
  }
  return sk_emit_mouse_button_at(abs, button, down);
}

bool sk_click_button_at_rel(SkPos pos, int button) {
  SkPos abs;
  if (!rel_to_abs(pos, &abs)) {
    return false;
  }
  return sk_click_button_at(abs, button);
}

void sk_emit_key(int vk, bool down) { Platform_EmitKey(vk, down ? 1 : 0); }

bool sk_replay_hotkey(SkKey key) {
  int mods = key.mods ? key.mods : 0;
  return hotkey_process(key.vk, mods);
}

bool sk_execute_hotkey(SkKey key) {
  bool handled = hotkey_execute(key.vk, key.mods);
  PromptState *state = current_prompt_state();
  if (!state)
    return handled;

  PopupKeyEvent ev = {0};
  ev.vk = key.vk;
  ev.ctrl = (key.mods & HOTKEY_MOD_CTRL) != 0;
  ev.shift = (key.mods & HOTKEY_MOD_SHIFT) != 0;
  ev.alt = (key.mods & HOTKEY_MOD_ALT) != 0;
  if (prompt_handle_key(state, &ev))
    return true;

  if (!ev.ctrl && !ev.alt && !(key.mods & HOTKEY_MOD_SUPER) && key.vk >= 32 &&
      key.vk < 127) {
    ev.text[0] = (char)key.vk;
    ev.text_len = 1;
    prompt_insert_text(state, ev.text, ev.text_len);
    return true;
  }
  return handled;
}

bool sk_type_text(const char *s) { return type_text_common(s); }

void sk_sleep(double seconds) { Platform_SleepSeconds(seconds); }

bool sk_color_matches(SkPos pos, SkColor color, int tolerance) {
  SkColor c = sk_get_pixel_color(pos);
  return pixel_color_matches_tol(c, color, tolerance);
}

bool sk_wait_pixel(SkPos pos, SkColor color, int tolerance, double timeout) {
  if (timeout <= 0.0)
    timeout = 10.0;
  int tol = tolerance > 0 ? tolerance : 0;
  int steps = (int)(timeout / 0.01);
  for (int i = 0;; i++) {
    SkColor c = sk_get_pixel_color(pos);
    if (pixel_color_matches_tol(c, color, tol))
      return true;
    if (i >= steps)
      return false;
    sk_sleep(0.01);
  }
}

SkLibrary sk_library_open(const char *path) {
  SkLibrary lib = (SkLibrary)Platform_LoadLibrary(path);
  if (!lib)
    return NULL;
  char canon[MAX_PATH];
  if (canonical_path(path, canon, sizeof(canon)))
    library_track_add(lib, canon);
  return lib;
}

void *sk_library_symbol(SkLibrary lib, const char *name) {
  return Platform_GetSymbol((LibHandle)lib, name);
}

void sk_library_close(SkLibrary lib) {
  if (!lib)
    return;
  char *caller =
      owner_resolve_for_func_alloc((void *)__builtin_return_address(0));
  char canon_caller[MAX_PATH];
  bool have_caller =
      caller && canonical_path(caller, canon_caller, sizeof(canon_caller));
  for (size_t i = 0; i < library_track_count(); i++) {
    if (library_track_lib(i) == lib && have_caller &&
        paths_equal(library_track_canon(i), canon_caller)) {
      log_error(
          "sk_library_close: refusing to unload the caller's own library");
      owner_strfree(&caller);
      return;
    }
  }
  for (size_t i = 0; i < library_track_count(); i++) {
    if (library_track_lib(i) == lib) {
      library_track_remove(i);
      break;
    }
  }
  Platform_UnloadLibrary((LibHandle)lib);
  owner_strfree(&caller);
}

const char *sk_library_error(void) { return Platform_GetLoadError(); }

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
  KeyEvent ev;
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
  MouseEvent ev;
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

bool sk_poll_scroll(SkScrollEvent *out) {
  if (!out)
    return false;
  ScrollEvent ev;
  if (!global_scroll_pop(&ev))
    return false;
  out->x = ev.x;
  out->y = ev.y;
  out->dx = ev.dx;
  out->dy = ev.dy;
  out->mods = ev.mods;
  out->t_us = ev.t_us;
  return true;
}

int sk_scroll_queue_len(void) { return global_scroll_pending(); }

void sk_clear_scroll_queue(void) { global_scroll_clear(); }

int sk_mouse_queue_len(void) { return global_mouse_pending(); }

unsigned long sk_mouse_queue_dropped(void) { return global_mouse_dropped(); }

void sk_clear_mouse_queue(void) { global_mouse_clear(); }

SkPos sk_get_cursor_pos(void) {
  int x, y;
  Platform_GetMousePos(&x, &y);
  return SK_POS(x, y);
}

SkPos sk_get_cursor_pos_rel(void) {
  int x, y;
  if (!Platform_GetMousePos(&x, &y))
    return SK_POS(0, 0);
  MonitorEntry monitors[16];
  int count = monitor_fill(monitors, 16);
  for (int i = 0; i < count; i++) {
    const MonitorEntry *m = &monitors[i];
    if (x >= m->x && x < m->x + m->w && y >= m->y && y < m->y + m->h)
      return SK_POS(x - m->x, y - m->y);
  }
  return SK_POS(x, y);
}

bool sk_mouse_pressed(int button) { return input_mouse_state_down(button); }

int sk_key_queue_len(void) { return global_key_pending(); }

unsigned long sk_key_queue_dropped(void) { return global_key_dropped(); }

void sk_clear_key_queue(void) { global_key_clear(); }

void sk_run_after_time(double seconds, void (*callback)(void)) {
  if (!callback || seconds < 0)
    return;
  SkRunAfterPayload *payload =
      (SkRunAfterPayload *)malloc(sizeof(SkRunAfterPayload));
  if (!payload)
    return;
  payload->seconds = seconds;
  payload->callback = callback;
  payload->owner = owner_resolve_for_func_alloc((void *)callback);
  if (!thread_registry_add(payload, payload->owner ? payload->owner : "")) {
    owner_strfree(&payload->owner);
    free(payload);
    return;
  }
  if (!sk_run_after_time_arm_platform(payload)) {
    thread_registry_finish(payload);
    owner_strfree(&payload->owner);
    free(payload);
  }
}

SkTimer sk_timer_start(void) { return Platform_StartTimer(); }

double sk_timer_end(SkTimer t) { return Platform_EndTimer(t); }

char *sk_get_active_window_title(void) {
  return owner_strdup(Platform_GetActiveWindowTitle());
}

char *sk_get_active_window_id(void) {
  return owner_strdup(Platform_GetActiveWindowId());
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
  if (!resolve_caller_path_silent(__builtin_return_address(0), name, full,
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

typedef struct {
  char name[64];
  SkLibrary handle;
} LibraryEntry;

static LibraryEntry *library_registry;
#define registry_count arrlenu(library_registry)

void sk_library_register(const char *name, SkLibrary handle) {
  if (!name || !handle)
    return;
  for (size_t i = 0; i < registry_count; ++i) {
    if (strcmp(library_registry[i].name, name) == 0) {
      library_registry[i].handle = handle;
      return;
    }
  }
  LibraryEntry *e = arraddnptr(library_registry, 1);
  safe_copy(e->name, sizeof(e->name), name);
  e->handle = handle;
}

SkLibrary sk_library_get(const char *name) {
  if (!name)
    return NULL;
  for (size_t i = 0; i < registry_count; ++i) {
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
  return widget_create(&copy, __builtin_return_address(0));
}

bool sk_widget_valid(SkWidget *widget) { return widget_valid(widget); }

bool sk_widget_poll_key(SkWidget *widget, SkWidgetKey *out) {
  return widget_poll_key(widget, out);
}

SkPos sk_get_widget_pos(const char *name) { return Widget_GetPos(name); }

void sk_widget_show(SkWidget *widget) { widget_show(widget); }

void sk_widget_hide(SkWidget *widget) { widget_hide(widget); }

void sk_widget_destroy(SkWidget *widget) { widget_destroy(widget); }

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
  safe_copy(out->name, sizeof(out->name), info.name);
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

int sk_parse_int(const char *text, int fallback) {
  int out = fallback;
  setting_parse_int(text, &out);
  return out;
}

float sk_parse_float(const char *text, float fallback) {
  float out = fallback;
  setting_parse_float(text, &out);
  return out;
}

bool ski_add_to_cc(const SkCcEntrySpec *spec) {
  return cc_add_from_spec(spec, __builtin_return_address(0));
}

SkSettingsEntry *sk_settings_entry_add(const SettingsEntrySpec *spec) {
  if (!spec)
    return NULL;
  settings_config_set_notify(settings_config_sync);
  SettingsEntrySpec copy = *spec;
  char *owner =
      owner_resolve_for_func_alloc((void *)__builtin_return_address(0));
  copy.owner = owner;
  char icon[MAX_PATH];
  if (resolve_icon_path(__builtin_return_address(0), spec->icon, icon,
                        sizeof(icon)))
    copy.icon = icon;
  SkSettingsEntry *result = settings_entry_add(&copy);
  owner_strfree(&owner);
  return result;
}

int ski_settings_entry_add_checkbox(SettingsEntry *entry,
                                    const SettingsCheckboxSpec *spec) {
  return settings_entry_add_checkbox(entry, spec);
}

int ski_settings_entry_add_button(SettingsEntry *entry,
                                  const SettingsButtonSpec *spec) {
  return settings_entry_add_button(entry, spec);
}

int ski_settings_entry_add_slider(SettingsEntry *entry,
                                  const SettingsSliderSpec *spec) {
  return settings_entry_add_slider(entry, spec);
}

int ski_settings_entry_add_dropdown(SettingsEntry *entry,
                                    const SettingsDropdownSpec *spec) {
  return settings_entry_add_dropdown(entry, spec);
}

SettingsEditboxGroup
ski_settings_entry_add_editbox(SettingsEntry *entry,
                               const SettingsEditboxSpec *spec) {
  return settings_entry_add_editbox(entry, spec);
}

const char *sk_settings_editbox_get(SettingsEditboxGroup group,
                                    const char *name) {
  return settings_editbox_get(group, name);
}

bool sk_settings_editbox_set(SettingsEditboxGroup group, const char *name,
                             const char *value) {
  return settings_editbox_set(group, name, value);
}

int sk_settings_editbox_right_offset(int pane_w, int width) {
  return settings_editbox_right_offset(pane_w, width);
}

int sk_settings_entry_count(void) { return settings_entry_count(); }

void sk_settings_scroll_to_block(const char *entry_name, int block_index) {
  settings_scroll_to_block(entry_name, block_index);
}

const SettingsEntrySpec *sk_settings_entry_at(int index) {
  return settings_entry_at(index);
}

int sk_cc_entry_count(void) { return cc_registry_count(); }

const SkCcEntrySpec *sk_cc_entry_at(int index) { return cc_registry_at(index); }

bool sk_register_alias(const char *alias, const char *target) {
  return alias_register(alias, target);
}

bool sk_remove_alias(const char *alias) { return alias_remove(alias); }

const char *sk_alias_for(const char *target) { return alias_for(target); }

void ski_window(const SkPromptSpec *spec) {
  SkaldContext *ctx = SkaldContext_Get();
  if (ctx && ctx->window.headless.active) {
    if (ctx->window.headless.capturing && spec) {
      ctx->window.headless.spec = *spec;
      ctx->window.headless.spec_valid = true;
      ctx->window.headless.capturing = false;
    }
    return;
  }
  WindowManager *wm = WindowManager_Get();
  if (AllowWindowOverride && wm && wm->popup_count > 0 && spec &&
      !spec->subwindow && (!ctx || !ctx->window.window_override_in_progress)) {
    if (ctx)
      ctx->window.window_override_in_progress = true;
    while (wm->popup_count > 0)
      WindowManager_PopPopup(wm);
    if (ctx)
      ctx->window.window_override_in_progress = false;
  }
  WindowManager_ShowPrompt(wm, spec);
}

const char *sk_get_user_name(void) { return Platform_GetRealUser(); }

bool sk_is_root(void) { return Platform_IsRunningAsRoot(); }

void sk_reload_feature(const char *name) { feature_loader_reload(name); }

int sk_get_pid(void) { return sk_get_pid_platform(); }

int sk_reload_all_features(void) {
  return feature_loader_build_stale_sources(true);
}

bool sk_hotkey_down(SkKey key) {
  if (!sk_is_key_down(key.vk))
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
  key_register_interceptor((KeyInterceptorFn)fn);
}

void sk_run_every(double seconds, void (*callback)(void)) {
  run_every_start(seconds, callback);
}

SkMetrics *sk_get_metrics(void) {
  PromptState *state = current_prompt_state();
  if (state && state->active_editbox >= 0 &&
      state->active_editbox < state->editbox_count)
    return &state->editboxes[state->active_editbox].metrics;
  return metrics_get();
}

void *sk_get_active_prompt(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (top && top->user_data && top->type == POPUP_PROMPT) {
    return top->user_data;
  }
  return NULL;
}

void sk_register_event(SkEvent event, void (*cb)(void)) {
  event_bus_register(event, cb);
}

void sk_unregister_event(SkEvent event, void (*cb)(void)) {
  event_bus_unregister(event, cb);
}

void sk_close_window(void) {
  SkaldContext_Get()->window.selection_complete = true;
}

bool ski_resize_window(const SkRect *rect) {
  if (!rect)
    return false;
  return prompt_popup_resize_current(rect->x, rect->y, rect->w, rect->h);
}

bool ski_resize_window_animated(const SkRect *rect, int duration_ms) {
  if (!rect)
    return false;
  return prompt_popup_resize_current_animated(rect->x, rect->y, rect->w,
                                              rect->h, duration_ms);
}

SkRect sk_get_window_bounds(void) {
  SkRect rect = {0};
  WindowManager *wm = WindowManager_Get();
  Popup *popup = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (!prompt_popup_get_bounds(popup, &rect.x, &rect.y, &rect.w, &rect.h))
    return (SkRect){0};
  return rect;
}

bool sk_run_feature_shortcut(const char *name) {
  if (!name || !name[0])
    return false;
  return feature_shortcut_run(name);
}

bool ski_config_set_hotkey(const SkConfigHotkeySpec *spec) {
  if (!spec)
    return false;
  return config_hotkey_set(spec->name, spec->hotkey, (void (*)(void *))spec->cb,
                           spec->ud);
}

char *sk_config_get_hotkey(const char *name) { return config_hotkey_get(name); }

bool sk_config_clear_hotkey(const char *name) {
  return config_hotkey_clear(name);
}

bool sk_config_bind_hotkey(const char *name, SkConfigFn cb, void *ud) {
  return config_hotkey_bind(name, (void (*)(void *))cb, ud);
}

int sk_config_hotkey_count(void) {
  return config_entry_count(CONFIG_ENTRY_HOTKEY);
}

char *sk_config_hotkey_name_at(int index) {
  return config_entry_dup_key_at(CONFIG_ENTRY_HOTKEY, index);
}

char *sk_config_hotkey_value_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_HOTKEY, index);
}

char *sk_config_hotkey_display_name_at(int index) {
  return config_hotkey_display_name_at(index);
}

bool ski_register_config_hotkey(const SkConfigHotkeyRegisterSpec *spec) {
  return config_hotkey_register((const ConfigHotkeyRegisterSpec *)spec);
}

void sk_init_all_hotkeys(void) { config_hotkey_bind_all(); }

bool sk_config_set_alias(const char *target, const char *alias) {
  return config_alias_set(target, alias);
}

char *sk_config_get_alias(const char *target) {
  return config_alias_get(target);
}

bool sk_config_clear_alias(const char *target) {
  return config_alias_set(target, NULL);
}

int sk_config_alias_count(void) {
  return config_entry_count(CONFIG_ENTRY_ALIAS);
}

char *sk_config_alias_target_at(int index) {
  return config_entry_dup_key_at(CONFIG_ENTRY_ALIAS, index);
}

char *sk_config_alias_value_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_ALIAS, index);
}

int sk_config_quicklink_count(void) {
  return config_entry_count(CONFIG_ENTRY_QUICKLINK);
}

char *sk_config_quicklink_name_at(int index) {
  return config_entry_dup_key_at(CONFIG_ENTRY_QUICKLINK, index);
}

char *sk_config_quicklink_value_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_QUICKLINK, index);
}

int sk_config_feature_shortcut_count(void) {
  return config_entry_count(CONFIG_ENTRY_FEATURE_SHORTCUT);
}

char *sk_config_feature_shortcut_name_at(int index) {
  return config_entry_dup_key_at(CONFIG_ENTRY_FEATURE_SHORTCUT, index);
}

char *sk_config_feature_shortcut_value_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_FEATURE_SHORTCUT, index);
}

bool sk_config_set_setting(const char *key, const char *value) {
  return config_setting_set(key, value);
}

bool sk_config_set_setting_int(const char *key, int value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", value);
  return sk_config_set_setting(key, buf);
}

bool sk_config_set_setting_float(const char *key, float value) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%.6g", value);
  return sk_config_set_setting(key, buf);
}

char *sk_config_get_setting(const char *key) { return config_setting_get(key); }

bool sk_config_get_setting_string(const char *key, char *out, size_t out_len,
                                  const char *fallback) {
  if (!out || out_len == 0)
    return false;
  char *saved = sk_config_get_setting(key);
  const char *value = (saved && saved[0]) ? saved : (fallback ? fallback : "");
  safe_copy(out, out_len, value);
  free(saved);
  return true;
}

bool sk_config_get_setting_bool(const char *key) {
  bool value = false;
  config_setting_get_value(key, config_parse_bool_value, &value);
  return value;
}

int sk_config_get_setting_int(const char *key, int fallback) {
  int value = fallback;
  config_setting_get_value(key, config_parse_int_value, &value);
  return value;
}

float sk_config_get_setting_float(const char *key) {
  return sk_config_get_setting_float_or(key, 0.0f);
}

float sk_config_get_setting_float_or(const char *key, float fallback) {
  float value = fallback;
  config_setting_get_value(key, config_parse_float_value, &value);
  return value;
}

bool sk_config_has_setting(const char *key) { return config_setting_has(key); }

int sk_config_setting_count(void) {
  return config_entry_count(CONFIG_ENTRY_SETTING);
}

const char *sk_config_setting_name_at(int index) {
  return config_entry_key_at(CONFIG_ENTRY_SETTING, index);
}

const char *sk_config_setting_value_at(int index) {
  return config_entry_value_at(CONFIG_ENTRY_SETTING, index);
}

bool sk_config_set_quicklink(const char *name, const char *target) {
  return config_quicklink_set(name, target);
}

bool sk_config_set_feature_shortcut(const char *name, const char *target) {
  return config_feature_shortcut_set(name, target);
}
