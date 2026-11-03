#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/checkbox.h"
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "platform/input_state.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/clipboard_history.h"
#include "core/flapi.h"
#include "core/feature_loader.h"
#include "core/cap_registry.h"
#include "helpers/helpers_owner.h"
#include "platform/flapi_platform.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/hotkey_string.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "platform/render_thread.h"
#include "platform/lib.h"
#include "deps/stb_ds.h"
#include "config_values.h"
#include "core/cc_registry.h"
#include "core/flapi_input.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/flapi_types.h"
#include "core/types/keys.h"
#include "core/types/window.h"

size_t fl_clipboard_history_count(void) {
  return clipboard_history_count_platform();
}

const FlClipboardEntry *fl_clipboard_history_get(size_t index) {
  return clipboard_history_get_platform(index);
}

const FlClipboardEntry *fl_clipboard_latest(void) {

  size_t n = clipboard_history_count_platform();
  if (n == 0)
    return NULL;
  return clipboard_history_get_platform(n - 1);
}

void fl_clipboard_clear_history(void) { clipboard_clear_history_platform(); }

bool fl_copy_to_clipboard(const char *text) {
  if (text == NULL)
    return 0;
  bool ok = Platform_CopyToClipboard(text);
  if (ok)
    clipboard_history_push_text_platform(text);
  return ok;
}

bool fl_copy_file_path(const char *path) {
  if (!path)
    return false;
  return clipboard_set_path_platform(path);
}

bool fl_get_all_files_from_dir(const char *dir_path,
                               char **list_of_allowed_extensions,
                               ListOfFiles *list_of_files,
                               char **list_of_names_to_ignore,
                               bool strip_extension, int max_depth) {

  if (!dir_path || !list_of_files)
    return false;

  char expanded_dir[MAX_PATH];
  if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir)))
    return false;
  dir_path = expanded_dir;
  return Platform_GetAllFilesFromDir(.dir_path = dir_path,
                                     .list_of_allowed_extensions = list_of_allowed_extensions,
                                     .list_of_files = list_of_files,
                                     .list_of_names_to_ignore =    list_of_names_to_ignore,
                                     .strip_extension = strip_extension,
                                     .max_depth = max_depth);
}

char *fl_search_file_in_dir(const char *directory, const char *search_string,
                            char *output_path) {

  if (!directory)
    return NULL;

  char expanded_dir[MAX_PATH];
  if (!Platform_ExpandPath(directory, expanded_dir, sizeof(expanded_dir)))
    return NULL;
  directory = expanded_dir;
  return fl_search_file_in_dir_platform(directory, search_string, output_path);
}

bool fl_launch_with_args(const char *path, const char *args) {
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) {
    return false;
  }
  bool r = fl_launch_with_args_platform(expanded, args);
  return r;
}

bool fl_launch_with_args_as_admin(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  return fl_launch_with_args_as_admin_platform(expanded, args);
}

bool fl_add_dep_impl(const char *dep_name, const char *owner_file) {
  return add_dep(dep_name, owner_file);
}

unsigned fl_cap_generation(void) { return cap_generation(); }
void fl_cap_register_abi(const char *key, unsigned abi, void *fn) {
  cap_register_abi(key, abi, fn);
}
void *fl_cap_resolve_abi(const char *key, unsigned abi) {
  return cap_resolve_abi(key, abi);
}
void fl_cap_bind_declare(const char *key, void *consumer) {
  cap_bind_declare(key, consumer);
}

FlStringList fl_load_file_list(const char *filepath) {
  FlStringList list = {0};
  if (!filepath)
    return list;
  char full[MAX_PATH];
  if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                sizeof(full))) {

    log_warn("fl_load_file_list resolve failed name='%s' feature='%s'",
             filepath,
             FlintContext_Get()->window.current_feature
                 ? FlintContext_Get()->window.current_feature
                 : "(null)");
    return list;
  }
  long budget = (long)MAX_FILE_LINES * (long)(MAX_FILE_LINE_LENGTH + 2);
  list = File_ReadLines(full, budget);
  if (list.count > MAX_FILE_LINES) {
    list.count = MAX_FILE_LINES;
    list.items[list.count] = NULL;
  }
  return list;
}

long fl_load_file(const char *filepath, char *buffer, long buffer_size) {
  if (!filepath || !buffer || buffer_size <= 0)
    return -1;
  buffer[0] = '\0';
  char full[MAX_PATH];
  if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full,
                                sizeof(full))) {

    log_warn("fl_load_file resolve failed name='%s' feature='%s'", filepath,
             FlintContext_Get()->window.current_feature
                 ? FlintContext_Get()->window.current_feature
                 : "(null)");
    return -1;
  }
  return File_ReadAll(full, buffer, buffer_size);
}

bool fl_free_list(const char *list[], int count) {
  if (!list || count <= 0)
    return false;
  for (int i = 0; i < count; i++)
    free((void *)list[i]);
  return true;
}

void fl_free_file_list(FlStringList *list) {
  if (!list) {
    return;
  }
  if (list->items) {
    for (int i = 0; i < list->count; i++)
      free(list->items[i]);
    free(list->items);
    list->items = NULL;
  }
  list->count = 0;
}
bool fl_free_window_list(FlWindowList *list) {
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

bool fl_append_to_file(const char *filepath, const char *line) {
  if (!filepath || !line)
    return false;
  char full[MAX_PATH];
  if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full,
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

bool fl_write_to_file(const char *filepath, const char *lines[], int count) {
  if (!filepath)
    return false;
  if (count < 0)
    return false;
  if (count > 0 && !lines)
    return false;
  char full[MAX_PATH];
  if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full,
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

bool fl_write_temp_file(const char *name, const void *data, size_t len,
                        char *out_path, size_t out_len) {
  return Platform_WriteTempFile(name, data, len, out_path, out_len);
}

char *fl_read_file_to_string(const char *path, size_t *out_len) {
  if (!path)
    return NULL;
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
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

long fl_read_entire_file(const char *full_path, char *buffer,
                         long buffer_size) {
  return File_ReadAll(full_path, buffer, buffer_size);
}

bool fl_write_string_to_file(const char *path, const char *content) {
  if (!path || !content)
    return false;
  char full[MAX_PATH];
  if (Platform_ResolveDataPath(__builtin_return_address(0), path, full,
                               sizeof(full))) {
    path = full;
  } else {
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
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

bool fl_file_exists(const char *path) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
    strncpy(expanded, path, sizeof(expanded) - 1);
  FILE *f = fopen(expanded, "rb");
  if (f) {
    fclose(f);
    return true;
  }
  return false;
}

bool fl_append_unique_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  FlStringList files = fl_load_file_list(path);
  bool ok = fl_append_to_file(path, line);
  fl_free_file_list(&files);
  return ok;
}

bool fl_remove_line(const char *path, const char *line) {
  if (!path || !line)
    return false;
  FlStringList files = fl_load_file_list(path);
  const char *kept[1024];
  int k = 0;
  for (int i = 0; i < files.count && k < 1024; i++)
    if (strcmp(files.items[i], line) != 0)
      kept[k++] = files.items[i];
    else {
      free(files.items[i]);
      files.items[i] = NULL;
    }
  bool ok = fl_write_to_file(path, kept, k);
  fl_free_file_list(&files);
  return ok;
}

bool fl_remove_line_contains(const char *path, const char *substr) {
  if (!path || !substr)
    return false;
  FlStringList files = fl_load_file_list(path);
  const char *kept[1024];
  int k = 0;
  for (int i = 0; i < files.count && k < 1024; i++)
    if (!strstr(files.items[i], substr))
      kept[k++] = files.items[i];
    else {
      free(files.items[i]);
      files.items[i] = NULL;
    }
  bool ok = fl_write_to_file(path, kept, k);
  fl_free_file_list(&files);
  return ok;
}

bool fl_parse_desktop_file(const char *path, char *out_name, size_t n,
                           bool *out_nodisplay) {

  if (!path || !out_name)
    return false;
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
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
    if (Platform_StringContainsCaseInsensitive(p, "NoDisplay=true") ||
        Platform_StringContainsCaseInsensitive(p, "Hidden=true")) {
      if (out_nodisplay)
        *out_nodisplay = true;
    }
    if (!found && Platform_StringContainsCaseInsensitive(p, "Name=")) {
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

bool fl_register_remapping_impl(const FlRemappingSpec *spec) {
  if (!spec || !spec->src || !spec->dst)
    return false;
  const char **wl = spec->whitelist;
  const char **bl = spec->blacklist;
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char **windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  return remap_register_internal(spec->src, spec->dst, whitelist, windows);
}

static void fl_snippet_fire(int idx, int del) {

  const SnippetBinding *b = snippet_get_binding(idx);
  if (!b || !b->trigger) {
    return;
  }
  for (int i = 0; i < del; i++) {
    Platform_EmitKey(VK_BACK, 1);
    Platform_EmitKey(VK_BACK, 0);
  }
  if (b->text)
    fl_type_text(b->text);
  else if (b->func)
    b->func();
}

bool fl_register_snippet_impl(const FlSnippetSpec *spec) {
  if (!spec || !spec->trigger || (!spec->text && !spec->fn))
    return false;
  snippet_set_fire_fn(fl_snippet_fire);
  const char **wl = spec->whitelist;
  const char **bl = spec->blacklist;
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char **windows = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  return snippet_register_internal(spec->trigger, spec->text, spec->fn,
                                   whitelist, windows,
                                   (void *)__builtin_return_address(0));
}

typedef struct {
  char *key;
  void *value;
} HashMapEnt;
typedef struct {
  char name[128];
  void (*cb)(void);
  char icon[512];
} FeatureReg;
static HashMap *g_feature_map = NULL;
static FeatureReg **g_feature_order = NULL;
static int g_feature_order_count = 0;

bool fl_register_hotkey_impl(const FlHotkeySpec *spec) {
  if (!spec || !spec->hotkey || !spec->cb)
    return false;
  const char **wl = spec->whitelist;
  const char **bl = spec->blacklist;
  bool whitelist = (wl && wl[0] && (!bl || !bl[0]));
  const char **bw = (bl && bl[0]) ? bl : ((wl && wl[0]) ? wl : NULL);
  HotkeyBinding binding = convert_into_hotkey_binding(
      (char *)spec->hotkey, spec->cb, NULL, bw, whitelist);
  binding.is_feature_toggle = false;
  binding.hold = true;
  binding.should_swallow = spec->should_swallow;
  if (spec->cb && g_feature_map) {
    for (int i = 0; i < g_feature_order_count; i++) {
      if (g_feature_order[i] &&
          (void *)g_feature_order[i]->cb == (void *)spec->cb) {
        binding.is_feature_toggle = true;
        break;
      }
    }
  }
  if (binding.is_feature_toggle)
    binding.hold = false;
  return hotkey_register_internal(binding);
}

int fl_get_feature_list(const char **out_names, void (**out_cbs)(void),
                        int max) {

  int n = g_feature_order_count < max ? g_feature_order_count : max;
  for (int i = 0; i < n; i++) {
    if (out_names)
      out_names[i] = g_feature_order[i]->name;
    if (out_cbs)
      out_cbs[i] = g_feature_order[i]->cb;
  }
  return n;
}

int fl_get_feature_list_with_icons(const char **out_names,
                                   void (**out_cbs)(void),
                                   const char ***out_icons, int max) {

  int n = g_feature_order_count < max ? g_feature_order_count : max;
  for (int i = 0; i < n; i++) {
    if (out_names)
      out_names[i] = g_feature_order[i]->name;
    if (out_cbs)
      out_cbs[i] = g_feature_order[i]->cb;
  }
  static const char **icon_ptrs = NULL;
  static int icon_ptrs_cap = 0;
  if (n > icon_ptrs_cap) {
    const char **new_ptrs =
        (const char **)realloc(icon_ptrs, n * sizeof(const char *));
    if (new_ptrs) {
      icon_ptrs = new_ptrs;
      icon_ptrs_cap = n;
    }
  }
  if (icon_ptrs) {
    for (int i = 0; i < n; i++)
      icon_ptrs[i] =
          g_feature_order[i]->icon[0] ? g_feature_order[i]->icon : NULL;
    if (out_icons)
      *out_icons = icon_ptrs;
  } else {
    if (out_icons)
      *out_icons = NULL;
  }
  return n;
}

char *fl_get_icon_path(const char *app_path_path) {
  if (!app_path_path)
    return NULL;
  return Platform_GetIconPath((char *)app_path_path);
}

int fl_get_icon_size(const char *icon_path) {
  if (!icon_path)
    return 20;
  return Platform_GetIconSize((char *)icon_path);
}

char *fl_run_terminal_command(char *command) {
  if (!command) {
    return NULL;
  }
  char *r = Platform_RunTerminalCommand(command, false);
  return r;
}

char *fl_run_argv(const char *const argv[], const char *stdin_data,
                  int *out_status) {

  if (!argv || !argv[0])
    return NULL;
  return Platform_RunArgvCapture(argv, stdin_data, out_status);
}

char *fl_run_terminal_command_blocking(char *command) {
  if (!command)
    return NULL;
  return Platform_RunCommandAndGetOutput(command, PLATFORM_RUN_PRIO_NORMAL);
}

char *fl_run_terminal_command_and_open(char *command) {
  if (!command) {
    return NULL;
  }
  char *r = Platform_RunTerminalCommand(command, true);
  return r;
}

bool fl_launch_detached(char *command) {
  if (!command) {
    return false;
  }
  bool r = Platform_LaunchDetached(command);
  return r;
}

bool fl_launch_terminal_at_path(char *path, char *args) {
  bool r = false;
  if (!path) {
    return false;
  }
  char expanded[MAX_PATH];
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) {
    return false;
  }
  r = Platform_LaunchTerminalAtPath(expanded, args);
  return r;
}

bool fl_terminate_process_by_name(const char *exe_name) {
  if (!exe_name)
    return false;
  return Platform_TerminateProcessByName(exe_name);
}
bool fl_string_contains(const char *haystack, const char *needle) {
  if (!needle || !haystack)
    return false;
  return Platform_StringContainsCaseInsensitive(needle, haystack);
}

char *fl_string_cut_at(const char *str, char delim) {
  return String_CutAt(str, delim);
}
char *fl_string_cut_before(const char *str, char delim) {
  return String_CutBefore(str, delim);
}
char *fl_string_cut_after(const char *str, char delim) {
  return String_CutAfter(str, delim);
}
char *fl_string_between(const char *str, char open, char close) {
  return String_Between(str, open, close);
}
char *fl_string_inside(const char *str, char delim) {
  return String_Inside(str, delim);
}
char *fl_string_trim(const char *str) { return String_Trim(str); }
char *fl_string_to_lower(const char *str) { return String_ToLower(str); }
char *fl_string_to_upper(const char *str) { return String_ToUpper(str); }
bool fl_string_starts_with(const char *str, const char *prefix) {
  return String_StartsWith(str, prefix);
}
bool fl_string_ends_with(const char *str, const char *suffix) {
  return String_EndsWith(str, suffix);
}
char *fl_string_replace(const char *str, const char *from, const char *to) {
  return String_Replace(str, from, to);
}
char *fl_string_slice(const char *str, int start, int end) {
  return String_Slice(str, start, end);
}

StringList fl_string_split(const char *str, const char *delimiter) {
  StringList empty = {NULL, 0};
  if (!str || !delimiter || !delimiter[0])
    return empty;
  return String_Split(str, delimiter);
}

void fl_string_list_free(StringList *list) { StringList_Free(list); }

void fl_populate_listbox(ListBoxInfo *lb, const char *filter,
                         const char **items, int count) {
  PopulateListBox(lb, filter, items, count, NULL);
}

void fl_populate_listbox_ex(ListBoxInfo *lb, const char *filter,
                            const char **items, int count,
                            bool (*filter_match)(const char *item,
                                                 const char *filter)) {
  PopulateListBox(lb, filter, items, count, filter_match);
}

bool fl_is_selected(ListBoxInfo *lb) {
  if (!lb)
    return false;
  return UI_IsListBoxSelected(lb);
}

void fl_set_edit_text(const char *text) {
  FlintContext *ctx = FlintContext_Get();
  if (!ctx) {
    return;
  }
  UI_SetText(text ? text : "");
}

void fl_set_edit_cursor(int position) {
  FlintContext *ctx = FlintContext_Get();
  if (!ctx) {
    return;
  }
  UI_SetCursorPosition(position);
}

int fl_get_edit_cursor(void) {
  FlintContext *ctx = FlintContext_Get();
  if (!ctx)
    return 0;
  int pos = UI_GetCursorPosition();
  return pos;
}

void fl_get_edit_text(char *buffer, size_t cap) {
  if (!buffer || cap == 0) {
    return;
  }
  buffer[0] = '\0';
  FlintContext *ctx = FlintContext_Get();
  if (!ctx) {
    return;
  }
  UI_GetCurrentTextEdit(buffer, cap);
}

static void fl_hover_popup_frame(void *ud) {

  FlHoverPopupCtx *p = (FlHoverPopupCtx *)ud;
  if (!p || !p->render) {
    return;
  }
  p->frame++;
  FlWidgetState st = {0};
  st.width = p->w;
  st.height = p->h;
  st.frame = p->frame;
  st.elapsed = fl_end_timer(p->created_at);
  st.hovered = true;
  Platform_SetDrawOffset(0, 0);
  Platform_BeginMeasure();
  p->render();
  int mw = 0, mh = 0;
  Platform_EndMeasure(&mw, &mh);
  if (mw > 0 && mh > 0) {
    p->w = mw + p->pad;
    p->h = mh + p->pad;
  }
}

static FlHoverPopupCtx *fl_hover_popup_ctx_new(const FlMessageSpec *spec) {

  FlHoverPopupCtx *p = (FlHoverPopupCtx *)calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->render = spec->render;
  p->w = spec->w > 0 ? spec->w : WIDGET_PROVISIONAL_W;
  p->h = spec->h > 0 ? spec->h : WIDGET_PROVISIONAL_H;
  p->created_at = fl_start_timer();
  p->pad = WIDGET_DEFAULT_PAD;
  return p;
}

static void fl_hover_popup_ctx_free(void *ud) {
  FlHoverPopupCtx *p = (FlHoverPopupCtx *)ud;
  if (p)
    free(p);
}

void fl_message_impl(const FlMessageSpec *spec) {
  if (!MessagesEnabled)
    return;
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  int mw = spec->mw > 0 ? spec->mw : 0;
  int mh = spec->mh > 0 ? spec->mh : 0;
  double timeout =  spec->timeout > 0 ? spec->timeout : (double)TimeOutForMessages;
  if (spec->close_on_keypress)  timeout = 6969.0;
  ColorRef color = spec->color_text ? spec->color_text : COLOR_TEXT;
  if (spec->hover_key && spec->hover_key[0]) {
    if (spec->render) {
      FlHoverPopupCtx *ctx = fl_hover_popup_ctx_new(spec);
      if (ctx) {
        Platform_HoverWidgetShow((PlHoverWidgetShowSpec){
            .key = spec->hover_key,
            .render = fl_hover_popup_frame,
            .ud = ctx,
            .x = spec->x,
            .y = spec->y,
            .w = ctx->w,
            .h = ctx->h,
            .fps = spec->fps > 0 ? spec->fps : 1,
            .no_bg = spec->no_bg,
            .bg_hex = spec->color_bg ? rgb_to_hex(spec->color_bg) : 0UL,
            .release = fl_hover_popup_ctx_free,
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

bool fl_launch_browser(char *url) {
  if (!url) {
    return false;
  }
  bool r = Platform_LaunchBrowserAtUrl(url, false);
  return r;
}

bool fl_query_browser(const char *query) {
  if (!query)
    return false;
  return Platform_QueryBrowser(query);
}

char *fl_get_last_clipboard_entry() {
  char *current = Platform_GetClipboardEntry();
  if (current) {
    if (current[0]) {
      const FlClipboardEntry *latest = fl_clipboard_latest();
      if (!latest || latest->type != CLIPBOARD_TEXT || !latest->text ||
          strcmp(latest->text, current) != 0)
        clipboard_history_push_text_platform(current);
    }
    free(current);
  }
  for (size_t i = clipboard_history_count_platform(); i > 0; i--) {
    const FlClipboardEntry *e = clipboard_history_get_platform(i - 1);
    if (e->type == CLIPBOARD_TEXT && e->text)
      return strdup(e->text);
  }
  return NULL;
}

bool fl_paste_from_clipboard() {
  char *content = Platform_GetClipboardEntry();
  return Platform_PasteContent(content);
}

bool fl_paste(char *content) {
  if (!content)
    return false;
  return Platform_PasteContent(content);
}

typedef struct {
  char *text;
  bool confirmed;
} PopupConfirmCtx;

static void fl_edit_blocking_confirm_ctx(const char *text, void *user_data) {
  PopupConfirmCtx *ctx = (PopupConfirmCtx *)user_data;
  if (ctx && text) {
    free(ctx->text);
    ctx->text = strdup(text);
    ctx->confirmed = true;
  }
}

char *fl_edit_text_popup_impl(const FlEditPopupSpec *spec) {
  const char *initial_text = spec ? spec->initial_text : NULL;
  int x = spec->x ? spec->x : 960;
  int y = spec->y ? spec->y : 540;
  int w = spec->w > 0 ? spec->w : 1000;
  int h = spec->h > 0 ? spec->h : 50;
  const char *align = spec->align ? spec->align : "LEFT";
  ListBoxInfo *lb = spec ? spec->lb : NULL;
  int *out_index = spec ? spec->out_index : NULL;
  if (out_index)
    *out_index = -1;
  if (!lb)
    return NULL;
  if (!initial_text)
    initial_text = "";
  int idx = lb->selected_index;
  int true_idx =
      (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  PopupConfirmCtx ctx = {0};
  Popup *popup = EditPopup_Create(wm, initial_text, x, y, w, h, align,
                                  fl_edit_blocking_confirm_ctx, &ctx);
  if (!popup)
    return NULL;
  WindowManager_PushPopup(wm, popup);
  while (wm->running && popup->manager && !ctx.confirmed) {
    Platform_WM_ProcessEvents(wm);
  }
  if (popup->manager != NULL) {
    WindowManager_PopPopup(wm);
  }
  if (lb && idx >= 0) {
    int keep_idx = idx < lb->populated_count ? idx : lb->populated_count - 1;
    if (keep_idx >= 0)
      lb->selected_index = keep_idx;
  }
  if (!ctx.confirmed) {
    free(ctx.text);
    ctx.text = NULL;
  }
  if (!ctx.text)
    return NULL;
  if (out_index)
    *out_index = true_idx;
  return ctx.text;
}

bool fl_replace_line_in_file(const char *filepath, int line_index,
                             const char *new_text) {
  if (!filepath || !new_text || line_index < 0)
    return false;
  FlStringList files = fl_load_file_list(filepath);
  if (line_index >= files.count) {

    fl_free_file_list(&files);
    return false;
  }
  free(files.items[line_index]);
  files.items[line_index] = NULL;
  files.items[line_index] = strdup(new_text);
  if (!files.items[line_index]) {

    fl_free_file_list(&files);
    return false;
  }
  bool ok = fl_write_to_file(filepath, (const char **)files.items, files.count);
  fl_free_file_list(&files);
  return ok;
}

bool fl_focus_window_by_title(char *title) {
  if (!title) {
    return false;
  }
  bool r = Platform_FocusWindow(title);
  return r;
}

bool fl_close_active_window(void) {
  bool r = Platform_CloseActiveWindow();
  return r;
}

bool fl_dismiss_message(const char *name) {
  bool r = fl_dismiss_message_platform(name);
  return r;
}

bool fl_message_active(const char *name) {
  bool r = Platform_MessageActive(name);
  return r;
}

bool fl_toggle_fullscreen_active(void) {
  bool r = Platform_ToggleFullscreenActive();
  return r;
}

bool fl_move_active_window_to_monitor(bool right) {
  bool r = Platform_MoveActiveWindowToMonitor(right);
  return r;
}

bool fl_move_active_window_left(void) {
  bool r = Platform_MoveActiveWindowLeft();
  return r;
}

bool fl_move_active_window_right(void) {
  bool r = Platform_MoveActiveWindowRight();
  return r;
}

bool fl_focus_window_left(void) {
  bool r = Platform_FocusWindowLeft();
  return r;
}

bool fl_focus_window_right(void) {
  bool r = Platform_FocusWindowRight();
  return r;
}

bool fl_toggle_minimized_active(void) {
  bool r = Platform_MinimizeActiveWindow();
  return r;
}

char *fl_popup_impl(const PopupSpec *spec) {
  if (!spec)
    return NULL;
  const char *initial_text = spec->initial_text ? spec->initial_text : "";
  int x = spec->x ? spec->x : 0;
  int y = spec->y ? spec->y : 0;
  int w = spec->w > 0 ? spec->w : 500;
  int h = spec->h > 0 ? spec->h : 70;
  const char *align = "LEFT";
  WindowManager *wm = WindowManager_Get();
  if (!wm) {
    ;
    return NULL;
  }
  PopupConfirmCtx ctx = {0};
  Popup *popup = EditPopup_Create(wm, initial_text, x, y, w, h, align,
                                  fl_edit_blocking_confirm_ctx, &ctx);
  if (!popup) {
    ;
    return NULL;
  }
  EditPopup_SetPlaceholder(popup, spec->placeholder);

  WindowManager_PushPopup(wm, popup);
  while (wm->running && popup->manager && !ctx.confirmed) {
    Platform_WM_ProcessEvents(wm);
  }
  if (popup->manager != NULL) {
    WindowManager_PopPopup(wm);
  }
  if (!ctx.confirmed) {
    free(ctx.text);
    ctx.text = NULL;
  }
  return ctx.text;
}

unsigned long fl_listbox_get_current_standard_color(void);

bool fl_draw_text_impl(const FlTextSpec *spec) {
  int x = spec->x ? spec->x : 0;
  int y = spec->y ? spec->y : 0;
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
    unsigned long cur = fl_listbox_get_current_standard_color();
    col = cur ? cur : rgb_to_hex(COLOR_TEXT);
  } else {
    col = rgb_to_hex(col);
  }
  const char *align = spec->align ? spec->align : "CENTER";
  FontHandle fh = spec->font;
  bool r =
      Platform_DrawTextRaw((PlDrawTextRawSpec){.x = spec->x,
                                               .y = spec->y,
                                               .text = real_text,
                                               .color = col,
                                               .font = fh,
                                               .align = (char *)align,
                                               .pad = spec->pad,
                                               .fontsize = spec->fontsize});
  return r;
}

typedef int (*FlWrapMeasureFn)(void *ctx, const char *s, int n);
typedef void (*FlWrapDrawFn)(void *ctx, int x, int y, const char *s, int n);
typedef struct {
  unsigned long color;
  int fontsize;
} FlWrapCtx;

static int FlUtf8LeadLen(unsigned char c) {

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

static void FlWrapRecord(FlCharPos *out, int maxout, int *count, int x, int y) {
  int i = *count;
  (*count)++;
  if (out && i < maxout) {
    out[i].x = x;
    out[i].y = y;
  }
}

static int FlWrapPlaceWord(FlWrapMeasureFn measure, FlWrapDrawFn draw,
                           void *ctx, const char *text, int ws, int we, int wx,
                           int rowtop, FlCharPos *out, int maxout, int *count) {

  int i = ws;
  while (i < we) {
    int j = i + FlUtf8LeadLen((unsigned char)text[i]);
    if (j > we)
      j = we;
    int px = wx + measure(ctx, text + ws, i - ws);
    for (int m = i; m < j; m++)
      FlWrapRecord(out, maxout, count, px, rowtop);
    i = j;
  }
  draw(ctx, wx, rowtop, text + ws, we - ws);
  return measure(ctx, text + ws, we - ws);
}

static int FlWrapLayout(const char *text, int x0, int y0, int max_width,
                        int line_height, FlWrapMeasureFn measure,
                        FlWrapDrawFn draw, void *ctx, FlCharPos *out,
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
      FlWrapRecord(out, maxout, &count, x0, y0 + (row + 1) * line_height);
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
      FlWrapRecord(out, maxout, &count, x0 + (has ? lw : 0),
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
          n = FlUtf8LeadLen((unsigned char)text[c]);
          if (n > we - c)
            n = we - c;
        }
        if (c + n >= we) {
          lw = FlWrapPlaceWord(measure, draw, ctx, text, c, we, x0, rowtop, out,
                               maxout, &count);
          has = true;
          c = we;
          break;
        }
        FlWrapPlaceWord(measure, draw, ctx, text, c, c + n, x0, rowtop, out,
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
    int pxw = FlWrapPlaceWord(measure, draw, ctx, text, ws, we, wx, rowtop, out,
                              maxout, &count);
    lw = has ? lw + spaceW + pxw : pxw;
    has = true;
    p = we;
  }
  return count;
}

static int FlWrapMeasureThunk(void *ctx, const char *s, int n) {

  FlWrapCtx *c = (FlWrapCtx *)ctx;
  return Platform_MeasureTextWidth(
      (PlMeasureTextWidthSpec){.text = s, .len = n, .fontsize = c->fontsize});
}

static void FlWrapNoDraw(void *ctx, int x, int y, const char *s, int n) {
  (void)ctx;
  (void)x;
  (void)y;
  (void)s;
  (void)n;
}

int fl_measure_text_wrapped_impl(const FlMeasureSpec *spec) {
  if (!spec || !spec->text) {
    return 0;
  }
  int fontsize = spec->fontsize;
  if (fontsize <= 0)
    fontsize = FontSize;
  int line_height = spec->line_height;
  if (line_height <= 0)
    line_height = fontsize + 4;
  FlWrapCtx ctx;
  ctx.color = 0;
  ctx.fontsize = fontsize;
  int cap = (int)strlen(spec->text) + 1;
  int r = FlWrapLayout(spec->text, spec->x, spec->y, spec->max_width,
                       line_height, FlWrapMeasureThunk, FlWrapNoDraw, &ctx,
                       spec->out_positions, cap);
  return r;
}

bool fl_place_editbox_impl(const FlEditSpec *spec) {
  if (!spec) {
    return false;
  }
  FlintContext_Get()->window.editbox_placed = true;
  bool r = fl_place_editbox_platform(spec);
  return r;
}

bool fl_draw_image_impl(const FlImageSpec *spec) {
  if (!spec || !spec->path || !spec->path[0]) {
    return false;
  }
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  char asset[MAX_PATH];
  const char *path = spec->path;
  if ((path[0] == '/' || path[0] == '\\') &&
      Platform_ResolveAssetPath(__builtin_return_address(0), path, asset,
                                sizeof(asset))) {
    path = asset;
  }
  bool r = Platform_DrawImageAt(path, spec->x, spec->y, w, h);
  return r;
}

bool fl_draw_rect_impl(const FlRectSpec *spec) {
  if (!spec) {
    return false;
  }
  unsigned long col = spec->color;
  bool transparent = spec->transparent ? spec->transparent : true;
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

int fl_rand_num_between(int min, int max) {
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

int fl_get_text_height(const char *text) {
  (void)text;
  int lh = 0, rh = 0, lt = 0;
  Platform_GetTextLayoutMetrics(&lh, &rh, &lt);
  int r = lh > 0 ? lh : FontSize;
  return r;
}

int fl_get_text_width(const char *text, int fontsize) {
  return Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = text, .len = (int)strlen(text), .fontsize = fontsize});
}

int fl_get_glyph_ink_height(const char *text, int fontsize) {
  int r = Platform_GetGlyphInkHeight(text, fontsize);
  return r;
}

char *fl_http_get(const char *url) { return Platform_HttpGet(url); }

char *fl_http_get_limited(const char *url, size_t max_bytes, size_t *out_len) {
  return Platform_HttpGetLimited(url, max_bytes, out_len);
}

bool fl_http_download(const char *url, const char *save_path) {
  return Platform_HttpDownload(url, save_path);
}

char *fl_http_post(const char *url, const char *content_type,
                   const char *body) {
  return Platform_HttpPost(url, content_type, body);
}

void fl_http_get_async(const char *url, FlHttpCallback cb, void *userdata) {
  Platform_HttpGetAsync(url, cb, userdata);
  return;
}

bool fl_draw_line_impl(const FlLineSpec *spec) {
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

bool fl_draw_circle_impl(const FlCircleSpec *spec) {
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

bool fl_listbox_begin(const FlListSpec *spec) {
  if (!spec) {
    return false;
  }
  bool r = fl_listbox_begin_platform(spec);
  return r;
}

bool fl_listbox_next(FlListItemCtx *out) {
  if (!out) {
    return false;
  }
  bool r = fl_listbox_next_platform(out);
  return r;
}

void fl_listbox_end(void) { fl_listbox_end_platform(); }
bool fl_place_listbox_impl(const FlListSpec *spec) {
  if (!spec) {
    return false;
  }
  if (!fl_listbox_begin(spec)) {
    return false;
  }
  FlListItemCtx ctx;
  while (fl_listbox_next(&ctx)) {
    fl_draw_text(.x = ctx.x + 10, .y = ctx.y + 4, .text = ctx.text,
                 .align = "LEFT", .pad = ctx.x + 10);
  }
  fl_listbox_end();
  return true;
}

bool fl_checkbox_is_checked(const char *name) {
  bool r = Checkbox_IsChecked(name);
  return r;
}

bool fl_checkbox_is_hovered(const char *name) {
  bool r = Checkbox_IsHovered(name);
  return r;
}

bool fl_place_checkbox_impl(const FlCheckboxSpec *spec) {
  bool r = Checkbox_Place(spec);
  return r;
}

FlWindowList fl_get_all_window_titles() {
  FlWindowList l = Platform_GetWindowTitles();
  return l;
}

FlWindowList fl_get_all_window_names() {
  FlWindowList l = Platform_GetWindowNames();
  return l;
}

PlColor fl_get_pixel_color(FlPos pos) {
  PlColor c = Platform_GetPixelColor(pos.x, pos.y);
  return c;
}
bool fl_move_to_pos(FlPos pos) {
  bool ok = Platform_MoveToPos(pos.x, pos.y);
  return ok;
}
bool fl_click_at_pos(FlPos pos) {
  bool ok = fl_move_to_pos(pos) && fl_click();
  return ok;
}
bool fl_left_click(void) {
  bool ok = Platform_ClickButton(1);
  return ok;
}
bool fl_right_click(void) {
  bool ok = Platform_ClickButton(2);
  return ok;
}
bool fl_emit_mouse_button(int button, bool down) {
  bool ok = Platform_MouseButtonDown(button, down);
  return ok;
}
bool fl_emit_mouse_button_at(FlPos pos, int button, bool down) {
  bool moved = fl_move_to_pos(pos);
  bool ok = moved && Platform_MouseButtonDown(button, down);
  return ok;
}
bool fl_click_button_at(FlPos pos, int button) {
  bool moved = fl_move_to_pos(pos);
  bool down = moved && Platform_MouseButtonDown(button, true);
  if (!down) {
    return false;
  }
  fl_sleep(0.02);
  bool up = Platform_MouseButtonDown(button, false);
  return up;
}
bool fl_mouse_scroll(int notches) {
  bool ok = Platform_MouseScroll(notches);
  return ok;
}

static bool fl_rel_to_abs(int rx, int ry, FlPos *out) {

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

bool fl_move_to_pos_rel(int rx, int ry) {
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = fl_move_to_pos(abs);
  return ok;
}

bool fl_click_at_pos_rel(int rx, int ry) {
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = fl_click_at_pos(abs);
  return ok;
}

bool fl_emit_mouse_button_at_rel(int rx, int ry, int button, bool down) {
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = fl_emit_mouse_button_at(abs, button, down);
  return ok;
}

bool fl_click_button_at_rel(int rx, int ry, int button) {
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {
    return false;
  }
  bool ok = fl_click_button_at(abs, button);
  return ok;
}

void fl_emit_key(int vk, bool down) { Platform_EmitKey(vk, down ? 1 : 0); }

bool fl_replay_hotkey(int vk, int mods) {
  bool matched = hotkey_process(vk, mods);
  return matched;
}
bool fl_type_text(const char *s) { return fl_type_text_platform(s); }

void fl_sleep(double seconds) { fl_sleep_platform(seconds); }

FlLibrary fl_library_open(const char *path) {
  return (FlLibrary)Platform_LoadLibrary(path);
}

void *fl_library_symbol(FlLibrary lib, const char *name) {
  return Platform_GetSymbol((LibHandle)lib, name);
}

void fl_library_close(FlLibrary lib) { Platform_UnloadLibrary((LibHandle)lib); }

const char *fl_library_error(void) { return Platform_GetLoadError(); }

FlThread fl_thread_create(FlThreadFunc fn, void *arg) {
  if (!fn)
    return NULL;
  return (FlThread)Platform_ThreadCreate((void *(*)(void *))fn, arg);
}

void fl_thread_join(FlThread thread) {
  if (!thread) {
    return;
  }
  Platform_ThreadJoin((void *)thread);
}

FlMutex fl_mutex_create(void) { return (FlMutex)Platform_MutexCreate(); }

void fl_mutex_destroy(FlMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexDestroy((void *)mutex);
}

void fl_mutex_lock(FlMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexLock((void *)mutex);
}

void fl_mutex_unlock(FlMutex mutex) {
  if (!mutex) {
    return;
  }
  Platform_MutexUnlock((void *)mutex);
}

void fl_send_hotkey(const char *hotkey_string) {
  if (!hotkey_string) {
    return;
  }
  remap_send_combo(hotkey_string);
}

static char g_fl_hotkey_string[HOTKEY_STRING_MAX_LEN];

char *fl_hotkey_to_string(FlKey key) {
  if (!HotkeyString_Format(key, g_fl_hotkey_string,
                           (int)sizeof(g_fl_hotkey_string))) {
    g_fl_hotkey_string[0] = '\0';
  }
  return g_fl_hotkey_string;
}

FlKey fl_hotkey_from_string(const char *str) {
  FlKey key = {0};
  HotkeyString_Parse(str, &key);
  return key;
}

bool fl_is_key_down(int vk) { return Platform_IsKeyDown(vk); }
bool fl_is_key_pressed(int vk) { return Platform_WasKeyPressed(vk); }
bool fl_is_key_released(int vk) { return Platform_WasKeyReleased(vk); }
bool fl_ctrl_pressed(void) { return Platform_IsCtrlPressed(); }
bool fl_shift_pressed(void) { return Platform_IsShiftPressed(); }
bool fl_alt_pressed(void) { return Platform_IsAltPressed(); }
bool fl_super_pressed(void) { return Platform_IsSuperPressed(); }
void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud) {
  Platform_SetKeyCallback(cb, ud);
}
int fl_get_key_pressed(void) { return Platform_GetPressedKey(); }
char fl_get_char_pressed(void) { return Platform_GetPressedChar(); }

bool fl_poll_key(FlKeyEvent *out) {
  if (!out)
    return false;
  PlatformKeyEvent ev;
  if (!Platform_GlobalKey_Pop(&ev))
    return false;
  out->vk = ev.vk;
  out->scancode = ev.scancode;
  out->mods = ev.mods;
  out->down = ev.down;
  out->repeat = ev.repeat;
  out->t_us = ev.t_us;
  return true;
}

bool fl_poll_mouse(FlMouseEvent *out) {
  if (!out)
    return false;
  PlatformMouseEvent ev;
  if (!Platform_GlobalMouse_Pop(&ev))
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

int fl_mouse_queue_len(void) { return Platform_GlobalMouse_Pending(); }

unsigned long fl_mouse_queue_dropped(void) {
  return Platform_GlobalMouse_Dropped();
}

void fl_clear_mouse_queue(void) { Platform_GlobalMouse_Clear(); }

FlPos fl_get_cursor_pos() {
  int x, y;
  Platform_GetMousePos(&x, &y);
  return (FlPos){x, y};
}

bool fl_mouse_pressed(int button) { return Platform_MouseStateDown(button); }

int fl_key_queue_len(void) { return Platform_GlobalKey_Pending(); }

unsigned long fl_key_queue_dropped(void) {
  return Platform_GlobalKey_Dropped();
}

void fl_clear_key_queue(void) { Platform_GlobalKey_Clear(); }

int fl_char_for_key(int vk, int scancode, int mods) {
  return Platform_CharForKey(vk, scancode, mods);
}

bool fl_is_flag_prefix(char c) { return c == '-' || c == '/'; }

size_t fl_extract_flag_name(char *p, char *out, size_t cap) {
  size_t n = 0;
  while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '=')
    n++;
  if (n >= cap)
    n = cap - 1;
  memcpy(out, p, n);
  out[n] = '\0';
  return n;
}

bool fl_extract_quoted(char **pp, char *out, size_t cap) {
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

bool fl_extract_value(char **pp, char *out, size_t cap) {
  char *p = *pp;
  if (*p == '"' || *p == '\'')
    return fl_extract_quoted(pp, out, cap);
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

bool fl_next_flag(char *p) {
  while (*p && isspace((unsigned char)*p))
    p++;
  return *p == '-' || *p == '/';
}

FlFlagList fl_parse_flags(const char *input) {
  FlFlagList out = {0};
  if (!input || !*input)
    return out;
  char *copy = strdup(input);
  if (!copy)
    return out;
  size_t cap = 8;
  FlFlag *arr = (FlFlag *)calloc(cap, sizeof(FlFlag));
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
    if (!fl_is_flag_prefix(*p)) {
      while (*p && !isspace((unsigned char)*p))
        p++;
      continue;
    }
    p++;
    if (*p == '-')
      p++;
    char name[64] = {0};
    size_t nlen = fl_extract_flag_name(p, name, sizeof(name));
    p += nlen;
    char valbuf[256] = {0};
    bool has_val = false;
    if (*p == '=') {
      p++;
      has_val = fl_extract_value(&p, valbuf, sizeof(valbuf));
    } else {
      char *save = p;
      while (*p && isspace((unsigned char)*p))
        p++;
      if (*p && !fl_is_flag_prefix(*p)) {
        has_val = fl_extract_value(&p, valbuf, sizeof(valbuf));
        if (has_val && fl_next_flag(p)) {
          has_val = false;
          p = save;
          while (*p && isspace((unsigned char)*p))
            p++;
        }
      }
    }
    if (out.count >= cap) {
      cap *= 2;
      FlFlag *narr = (FlFlag *)realloc(arr, cap * sizeof(FlFlag));
      if (!narr)
        break;
      arr = narr;
    }
    FlFlag *f = &arr[out.count++];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, sizeof(f->name) - 1);
    if (has_val) {
      strncpy(f->value, valbuf, sizeof(f->value) - 1);
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

void fl_free_flags(FlFlagList *list) {
  if (!list) {
    return;
  }
  free(list->flags);
  list->flags = NULL;
  list->count = 0;
}

void fl_run_after_time(double seconds, void (*callback)(void)) {
  return fl_run_after_time_platform(seconds, callback);
}

FlTimer fl_start_timer(void) { return Platform_StartTimer(); }

double fl_end_timer(FlTimer t) { return Platform_EndTimer(t); }

char *fl_get_active_window_title(void) {
  char *title = Platform_GetActiveWindowTitle();
  if (!title || !title[0]) {
    return NULL;
  }
  char *r = strdup(title);
  return r;
}

char *fl_get_active_window_app_id(void) {
  char *id = Platform_GetActiveWindowAppId();
  if (!id || !id[0]) {
    return NULL;
  }
  char *r = strdup(id);
  return r;
}

bool fl_is_window_active(char *title) {
  FlWindowList l = Platform_GetWindowTitles();
  for (int i = 0; i < l.count; i++) {
    if (Platform_StringContainsCaseInsensitive(l.windows[i], title)) {
      return true;
    }
  }
  return false;
}

char *fl_get_working_dir() { return Platform_GetWorkingDir(); }

char *fl_resolve_data_path(const char *name) {
  if (!name || !name[0])
    return NULL;
  char full[MAX_PATH];
  if (!Platform_ResolveDataPath(__builtin_return_address(0), name, full,
                                sizeof(full)))
    return NULL;
  char *r = (char *)malloc(strlen(full) + 1);
  if (!r)
    return NULL;
  strcpy(r, full);
  return r;
}

char *fl_resolve_feature_path(const char *name) {
  char buf[340];
  int len = snprintf(buf, sizeof(buf), "%s/%s", FeaturePath, name);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return NULL;
  }

  char *res = (char *)malloc(len + 1);
  if (!res) {
    return NULL;
  }

  strcpy(res, buf);
  return res;
}

char *fl_resolve_user_path() { return Platform_GetRealHome(); }

void fl_hashmap_init(FlHashMap **map) {
  if (!map) {
    return;
  }
  sh_new_strdup(*map);
}

void fl_hashmap_free(FlHashMap **map) {
  if (!map) {
    return;
  }
  shfree(*map);
}

void fl_hashmap_put(FlHashMap **map, const char *key, long value) {
  if (!map || !key) {
    return;
  }
  shput(*map, (char *)key, value);
}

void fl_hashmap_add(FlHashMap **map, const char *key, long delta) {
  if (!map || !key) {
    return;
  }
  ptrdiff_t idx = shgeti(*map, (char *)key);
  if (idx >= 0)
    (*map)[idx].value += delta;
  else
    shput(*map, (char *)key, delta);
}

long fl_hashmap_get(FlHashMap *map, const char *key, long def) {
  if (!map || !key)
    return def;
  ptrdiff_t idx = shgeti(map, (char *)key);
  if (idx < 0)
    return def;
  return map[idx].value;
}

bool fl_hashmap_has(FlHashMap *map, const char *key) {
  if (!map || !key)
    return false;
  return shgeti(map, (char *)key) >= 0;
}

bool fl_hashmap_del(FlHashMap **map, const char *key) {
  if (!map || !*map || !key)
    return false;
  return shdel(*map, (char *)key) != 0;
}

size_t fl_hashmap_len(FlHashMap *map) {
  if (!map)
    return 0;
  return shlenu(map);
}

const FlHashMap *fl_hashmap_at(FlHashMap *map, size_t index) {

  if (!map || index >= shlenu(map))
    return NULL;
  return &map[index];
}

FlTimer fl_profile_start() { return fl_start_timer(); }

void fl_profile_end(FlTimer timer) {
  double ms = fl_end_timer(timer);
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

static FlWidget *g_fl_widgets[WIDGET_MAX];
static int g_fl_widget_count = 0;

typedef struct {
  char name[64];
  FlLibrary handle;
} LibraryEntry;
static LibraryEntry g_library_registry[32];
static int g_registry_count = 0;

void fl_library_register(const char *name, FlLibrary handle) {
  if (!name || !handle)
    return;
  for (int i = 0; i < g_registry_count; ++i) {
    if (strcmp(g_library_registry[i].name, name) == 0) {
      g_library_registry[i].handle = handle;
      return;
    }
  }
  if (g_registry_count < 32) {
    strncpy(g_library_registry[g_registry_count].name, name, 63);
    g_library_registry[g_registry_count].handle = handle;
    g_registry_count++;
  }
}

FlLibrary fl_library_get(const char *name) {
  if (!name)
    return NULL;
  for (int i = 0; i < g_registry_count; ++i) {
    if (strcmp(g_library_registry[i].name, name) == 0)
      return g_library_registry[i].handle;
  }
  return NULL;
}

static FlTimer g_mon_at;
static bool g_mon_valid = false;
static int g_mon_x, g_mon_y, g_mon_w, g_mon_h;

static void fl_widget_monitor_geometry(int *x, int *y, int *w, int *h) {

  if (!g_mon_valid || fl_end_timer(g_mon_at) > 2000) {
    Platform_WM_GetMonitorGeometry(&g_mon_x, &g_mon_y, &g_mon_w, &g_mon_h);
    g_mon_at = fl_start_timer();
    g_mon_valid = true;
  }
  *x = g_mon_x;
  *y = g_mon_y;
  *w = g_mon_w;
  *h = g_mon_h;
}

static void fl_widget_relayout(void);

static FlPos g_fl_hover_pos = {0, 0};
static FlTimer g_fl_hover_at = {0};
static bool g_fl_hover_valid = false;

static void fl_widget_hover_poll(void) {

  FlTimer now = fl_start_timer();
  if (g_fl_hover_valid && fl_end_timer(g_fl_hover_at) < 400.0) {
    return;
  }
  g_fl_hover_pos = fl_get_cursor_pos();
  g_fl_hover_at = now;
  g_fl_hover_valid = true;
}

static void *fl_widget_hover_worker(void *ud) {

  FlWidget *w = (FlWidget *)ud;
  if (w && w->on_hover)
    w->on_hover();
  if (w)
    w->hover_running = false;
  return NULL;
}

static void fl_widget_render_frame(void *ud) {

  FlWidget *widget = (FlWidget *)ud;
  if (!widget || !widget->render || widget->dead) {
    {
      return;
    }
  }
  widget->frame++;
  bool hoverable = (widget->on_hover != NULL);
  if (hoverable) {
    fl_widget_hover_poll();
    bool inside = (widget->x >= 0x7fffffff)
                      ? false
                      : (g_fl_hover_pos.x >= widget->x &&
                         g_fl_hover_pos.x < widget->x + widget->w &&
                         g_fl_hover_pos.y >= widget->y &&
                         g_fl_hover_pos.y < widget->y + widget->h);
    if (inside && !widget->hovered && widget->on_hover &&
        !widget->hover_running) {
      widget->hover_running = true;
      widget->hover_thread = fl_thread_create(fl_widget_hover_worker, widget);
      if (!widget->hover_thread)
        widget->hover_running = false;
    }
    widget->hovered = inside;
    if (!inside) {
      Platform_HoverMessageRelease(widget->name);
      Platform_HoverWidgetHide(widget->name);
    }
  }
  FlWidgetState st;
  st.width = widget->w;
  st.height = widget->h;
  st.frame = widget->frame;
  st.elapsed = fl_end_timer(widget->created_at);
  st.hovered = widget->hovered;

  Platform_SetDrawOffset(-widget->origin_x, 0);
  Platform_BeginMeasure();
  widget->render(&st);
  int mw = 0, mh = 0, mx = 0, my = 0;
  Platform_EndMeasure(&mw, &mh);
  Platform_GetLastMeasureOrigin(&mx, &my);
  mx += widget->origin_x;

  if (widget->origin_x != mx || widget->origin_y != my) {
    widget->origin_x = mx;
    widget->origin_y = my;
    Platform_Widget_Invalidate(widget->win);
  }

  if (widget->auto_w || widget->auto_h) {
    int nw = (widget->auto_w && mw > 0) ? mw + widget->pad : widget->w;
    int nh = (widget->auto_h && mh > 0) ? mh + widget->pad : widget->h;

    if (nw != widget->w || nh != widget->h) {
      widget->w = nw;
      widget->h = nh;
      if (Platform_Widget_IsValid(widget->win)) {
        Platform_Widget_Resize(widget->win, nw, nh);
      }
      fl_widget_relayout();
    }
  }
  char prof_label[80];
  snprintf(prof_label, sizeof(prof_label), "flapi:widget[%s]", widget->name);
}

static void fl_widget_attach_window(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (!widget || widget->dead)
    return;
  bool created = false;
  if (!Platform_Widget_IsValid(widget->win)) {
    Platform_SetDrawBgColor(widget->has_bg ? widget->bg_color : 0);
    widget->win = Platform_Widget_Create(widget->x, widget->y, widget->w,
                                         widget->h, widget->fps, widget->no_bg,
                                         widget->on_click, widget->name);
    Platform_SetDrawBgColor(0);
    created = (widget->win != NULL);
  } else {
    Platform_Widget_Move(widget->win, widget->x, widget->y);
  }
  if (widget->win) {
    Platform_Widget_SetBgColor(widget->win,
                               widget->has_bg ? widget->bg_color : 0);
    Platform_Widget_SetNoBg(widget->win, widget->no_bg);
    Platform_Widget_SetFps(widget->win, widget->fps);
    if (created)
      Platform_Widget_SetFrameCallback(widget->win, fl_widget_render_frame,
                                       widget);
  }
}

static void fl_widget_apply_geometry(FlWidget *widget, int x, int y) {
  widget->x = x;
  widget->y = y;
  if (Platform_RenderThread_IsCurrentThread()) {

    fl_widget_attach_window(widget);
    return;
  }
  if (!Platform_RenderThread_EnsureStarted(widget->fps)) {
    return;
  }
  if (!Platform_RenderThread_Post(fl_widget_attach_window, widget)) {
    return;
  }
}

static int fl_widget_sort_key(const FlWidget *widget) {
  return widget->order_set ? widget->order : widget->seq;
}

static void fl_widget_relayout(void) {

  int mx = 0, my = 0, mw = 1920, mh = 1080;
  fl_widget_monitor_geometry(&mx, &my, &mw, &mh);

  FlWidget *sorted[WIDGET_MAX];
  int n = 0;
  for (int i = 0; i < g_fl_widget_count; i++) {
    if (g_fl_widgets[i] && !g_fl_widgets[i]->free_pos)
      sorted[n++] = g_fl_widgets[i];
  }
  for (int i = 1; i < n; i++) {
    FlWidget *key = sorted[i];
    int k = i - 1;
    while (k >= 0 && fl_widget_sort_key(sorted[k]) > fl_widget_sort_key(key)) {
      sorted[k + 1] = sorted[k];
      k--;
    }
    sorted[k + 1] = key;
  }

  for (int s = 0; s < 4; s++) {
    bool from_right =
        (s == WIDGET_SLOT_TOP_RIGHT || s == WIDGET_SLOT_BOTTOM_RIGHT);
    bool from_top =
        (s == WIDGET_SLOT_TOP_LEFT || s == WIDGET_SLOT_TOP_RIGHT);
    int cursor = from_right ? mx + mw : mx;
    for (int i = 0; i < n; i++) {
      FlWidget *w = sorted[i];
      if (w->slot != s)
        continue;
      int g = w->gap_override > 0 ? w->gap_override : WidgetGap;
      int x = from_right ? cursor - w->w : cursor;
      int y = from_top ? my : my + mh - w->h;
      fl_widget_apply_geometry(w, x, y);
      cursor += from_right ? -(w->w + g) : (w->w + g);
    }
  }
}

FlWidget *fl_widget_create(const FlWidgetSpec *spec) {
  return widget_create(spec);
}

bool fl_widget_valid(FlWidget *widget) { return widget_valid(widget); }

FlPos fl_get_widget_pos(const char *name) { return widget_get_pos(name); }

void fl_widget_show(FlWidget *widget) { widget_show(widget); }

void fl_widget_hide(FlWidget *widget) { widget_hide(widget); }

void fl_widget_set_render(FlWidget *widget, FlWidgetRenderFn render) {
    widget_set_render(widget, render);
}

void fl_format_bytes(unsigned long long bytes, char *buf, size_t buflen) {
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
FlNetStatus fl_net_status(void) { return fl_net_status_platform(); }

unsigned long long fl_net_rx_bytes() { return fl_net_rx_bytes_platform(); }

unsigned long long fl_net_rx_bytes_over(double seconds) {

  if (seconds <= 0)
    return 0;

  unsigned long long before = fl_net_rx_bytes_platform();
  if (before == 0)
    return 0;

  fl_sleep(seconds);

  unsigned long long after = fl_net_rx_bytes_platform();
  if (after < before)
    return 0;
  return after - before;
}

FlRect fl_get_monitor_geometry() {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return (PlRect){x, y, w, h};
}

int fl_get_screen_width() {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return w;
}

int fl_get_screen_height() {
  int x, y, w, h;
  WindowManager_GetMainMonitorGeometry(&x, &y, &w, &h);
  return h;
}

typedef struct {
  FlCcEntrySpec pub;
  char display_name[1024];
  char internal_name[1024];
  char icon[1024];
  char type[128];
  char owner[OWNER_PATH_MAX];
} FlCcEntry;

static int g_cc_entry_count = 0;
static FlCcEntry g_cc_entries[4096];

#define FL_CC_DEP_OWNERS 256
static char g_cc_dep_owners[FL_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int g_cc_dep_owner_count = 0;

static bool fl_cc_dep_seen(const char *owner) {
  for (int i = 0; i < g_cc_dep_owner_count; i++)
    if (strcmp(g_cc_dep_owners[i], owner) == 0)
      return true;
  return false;
}

static void fl_cc_dep_mark(const char *owner) {
  if (g_cc_dep_owner_count >= FL_CC_DEP_OWNERS)
    return;
  snprintf(g_cc_dep_owners[g_cc_dep_owner_count++], OWNER_PATH_MAX, "%s",
           owner);
}

static const char *fl_cc_dup(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src ? src : "");
  return dst;
}

bool fl_add_to_cc_impl(const FlCcEntrySpec *spec) {
  if (!spec)
    return false;
  int slot = -1;
  for (int i = 0; i < g_cc_entry_count; i++) {
    if (spec->display_name && g_cc_entries[i].display_name[0] &&
        strcmp(g_cc_entries[i].display_name, spec->display_name) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    if (g_cc_entry_count >= 4096)
      return false;
    slot = g_cc_entry_count++;
  }
  FlCcEntry *e = &g_cc_entries[slot];
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
      fl_cc_dup(e->display_name, sizeof(e->display_name), spec->display_name);
  char icon_asset[MAX_PATH];
  if (spec->icon && (spec->icon[0] == '/' || spec->icon[0] == '\\') &&
      Platform_ResolveAssetPath(__builtin_return_address(0), spec->icon,
                                icon_asset, sizeof(icon_asset))) {
    e->pub.icon = fl_cc_dup(e->icon, sizeof(e->icon), icon_asset);
  } else {
    e->pub.icon = fl_cc_dup(e->icon, sizeof(e->icon), spec->icon);
  }
  e->pub.type = fl_cc_dup(e->type, sizeof(e->type), spec->type);
  if (spec->internal_name) {
    e->pub.internal_name = fl_cc_dup(e->internal_name, sizeof(e->internal_name),
                                     spec->internal_name);
  } else {
    e->pub.internal_name = fl_cc_dup(e->internal_name, sizeof(e->internal_name),
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
  if (e->owner[0] && !fl_cc_dep_seen(e->owner)) {
    fl_cc_dep_mark(e->owner);
    add_dep("control_center.c", e->owner);
  }
  return true;
}

int fl_cc_entry_count(void) { return g_cc_entry_count; }

const FlCcEntrySpec *fl_cc_entry_at(int index) {
  if (index < 0 || index >= g_cc_entry_count)
    return NULL;
  return &g_cc_entries[index].pub;
}

#define FL_MAX_ALIASES 1024
static char g_alias_alias[FL_MAX_ALIASES][128];
static char g_alias_target[FL_MAX_ALIASES][256];
static int g_alias_count = 0;

bool fl_add_alias_impl(const FlAliasSpec *spec) {
  if (!spec || !spec->alias || !spec->target)
    return false;
  if (g_alias_count >= FL_MAX_ALIASES)
    return false;
  for (int i = 0; i < g_alias_count; i++) {
    if (strcmp(g_alias_target[i], spec->target) == 0) {
      snprintf(g_alias_alias[i], sizeof(g_alias_alias[i]), "%s", spec->alias);
      return true;
    }
  }
  snprintf(g_alias_alias[g_alias_count], sizeof(g_alias_alias[g_alias_count]),
           "%s", spec->alias);
  snprintf(g_alias_target[g_alias_count], sizeof(g_alias_target[g_alias_count]),
           "%s", spec->target);
  g_alias_count++;
  return true;
}

const char *fl_alias_for(const char *target) {
  if (!target)
    return NULL;
  for (int i = 0; i < g_alias_count; i++) {
    if (strcmp(g_alias_target[i], target) == 0)
      return g_alias_alias[i];
  }
  return NULL;
}

void fl_window_impl(const FlPromptSpec *spec) {
  WindowManager_ShowPrompt(WindowManager_Get(), spec);
}

char *fl_get_user_name() { return Platform_GetRealUser(); }

bool fl_is_root() { return Platform_IsRunningAsRoot(); }
void fl_reload_feature(char *name) { FeatureLoader_Reload(name); }

int fl_get_pid() { return fl_get_pid_platform(); }

void fl_reload_all_features(void) {
  if (!FeaturePath || !FeaturePath[0])
    return;
  DIR *dir = opendir(FeaturePath);
  if (!dir)
    return;
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
    snprintf(dll, sizeof(dll), "build/features/%s.so", ent->d_name);
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
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", dll, fl_get_pid());
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "gcc -I include -pipe -fPIC -shared -o \"%s\" \"%s\" 2>&1", tmp,
             src);
    char *outStr = fl_run_terminal_command(cmd);
    bool ok = false;
    if (outStr) {
      if (strlen(outStr) == 0 || (!strstr(outStr, "error:") &&
                                  !strstr(outStr, "undefined reference") &&
                                  !strstr(outStr, "failed"))) {
        ok = true;
      }
      free(outStr);
    } else {
      ok = true;
    }
    if (!ok) {
      remove(tmp);
      continue;
    }
    remove(dll);
    rename(tmp, dll);
    fl_reload_feature(ent->d_name);
    reloaded++;
  }
  closedir(dir);
  char msg[128];
  snprintf(msg, sizeof(msg), "Reloaded %d feature(s)!", reloaded);
  fl_message(.message = msg, .slot = BOTTOM_CENTER, .timeout = 1.0);
}

bool fl_hotkey_down(FlKey key) {
  if (!fl_is_key_down(key.vk_code))
    return false;
  if ((key.mods & FL_CTRL) && !fl_ctrl_pressed())
    return false;
  if ((key.mods & FL_SHIFT) && !fl_shift_pressed())
    return false;
  if ((key.mods & FL_ALT) && !fl_alt_pressed())
    return false;
  if ((key.mods & FL_SUPER) && !fl_super_pressed())
    return false;
  return true;
}

void fl_register_key_interceptor(FlKeyInterceptorFn fn) {
  Platform_RegisterKeyInterceptor((PlatformKeyInterceptorFn)fn);
}

FlMetrics *fl_get_metrics(void) {
  return Platform_GetMetrics();
}
void *fl_get_active_prompt(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (top && top->user_data && top->type == POPUP_PROMPT) {
    return top->user_data;
  }
  return NULL;
}

void fl_register_event(FlEvent event, FlEventCallback cb) {
  EventBus_Register(event, cb);
}

void fl_unregister_event(FlEvent event, FlEventCallback cb) {
  EventBus_Unregister(event, cb);
}
