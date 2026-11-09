#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "core/types/window.h"
#include "core/types.h"
#include "platform/platform.h"
#include "core/hotkey_string.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_text.h"
#include "core/types/keys.h"
#include "core/types/file.h"
#include "deps/log.h"

HotkeyBinding
convert_into_hotkey_binding(char *hotkey_string,
                            void (*on_hotkey_trigger)(void),
                            bool (*optional_condition_for_hotkey)(void),
                            const char *const *optional_blacklistedwindows,
                            bool whitelist, void *user_ud) {
  HotkeyBinding output = {0};
  HotkeyKey key = {0};

  HotkeyString_Parse(hotkey_string, &key);

  output.vk_code = key.vk_code;
  output.modifiers = key.mods;
  output.hotkey_callback = on_hotkey_trigger;
  output.hotkey_condition = optional_condition_for_hotkey;
  output.whitelist = whitelist;
  output.user_ud = user_ud;

  if (optional_blacklistedwindows) {
    int cnt = 0;
    while (optional_blacklistedwindows[cnt] != NULL)
      cnt++;
    output.blacklist_count = cnt;

    if (cnt > 0) {

      output.blacklist = malloc(sizeof(char *) * cnt);
      for (int i = 0; i < cnt; i++) {

        output.blacklist[i] = strdup(optional_blacklistedwindows[i]);
      }
    } else {
      output.blacklist = NULL;
    }
  } else {
    output.blacklist = NULL;
    output.blacklist_count = 0;
  }
  return output;
}

StringList String_Split(const char *string, const char *delimiter) {
  StringList result = {0};

  if (!string || !delimiter || delimiter[0] == '\0')
    return result;

  size_t delimiter_count = 0;
  const char *p = string;

  while ((p = strstr(p, delimiter)) != NULL) {
    delimiter_count++;
    p += strlen(delimiter);
  }

  result.count = delimiter_count + 1;
  result.items = malloc((result.count + 1) * sizeof(char *));

  if (!result.items) {
    result.count = 0;
    return result;
  }

  result.items[result.count] = NULL;

  size_t delimiter_len = strlen(delimiter);
  const char *start = string;

  for (size_t i = 0; i < result.count; i++) {
    const char *end = strstr(start, delimiter);

    size_t length;

    if (end)
      length = (size_t)(end - start);
    else
      length = strlen(start);

    result.items[i] = malloc(length + 1);

    if (!result.items[i]) {
      for (size_t j = 0; j < i; j++)
        free(result.items[j]);

      free(result.items);
      result.items = NULL;
      result.count = 0;
      return result;
    }

    memcpy(result.items[i], start, length);
    result.items[i][length] = '\0';

    if (!end)
      break;

    start = end + delimiter_len;
  }
  return result;
}

void StringList_Free(StringList *list) {
  if (!list)
    return;

  for (size_t i = 0; i < list->count; i++)
    free(list->items[i]);

  free(list->items);

  list->items = NULL;
  list->count = 0;
}

static const char *resolve_home_path(const char *path, char *expanded,
                                     size_t cap) {
  if (!(path[0] == '~' && (path[1] == '/' || path[1] == '\0')))
    return path;

  const char *home = Platform_GetRealHome();

  if (!home || !home[0])
    return NULL;

  size_t hn = strlen(home);

  if (hn >= cap)
    return NULL;

  memcpy(expanded, home, hn);
  size_t total = hn;

  const char *rest = path + 1;

  while (*rest && total < cap - 1)
    expanded[total++] = *rest++;

  expanded[total] = '\0';
  return expanded;
}

static char *read_all_dynamic(const char *path) {
  char expanded[4096];
  path = resolve_home_path(path, expanded, sizeof(expanded));
  if (!path)
    return NULL;

  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  size_t cap = 8192;
  size_t len = 0;
  char *buf = (char *)malloc(cap);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  for (;;) {
    if (len + 1 >= cap) {
      size_t ncap = cap * 2;
      char *grown = (char *)realloc(buf, ncap);
      if (!grown) {
        free(buf);
        fclose(f);
        return NULL;
      }
      buf = grown;
      cap = ncap;
    }
    size_t got = fread(buf + len, 1, cap - len - 1, f);
    if (got == 0)
      break;
    len += got;
  }

  fclose(f);
  buf[len] = '\0';
  return buf;
}

long File_ReadAll(const char *path, char *buffer, long buffer_size) {
  if (!path || !buffer || buffer_size <= 0)
    return -1;

  buffer[0] = '\0';

  char expanded[4096];
  path = resolve_home_path(path, expanded, sizeof(expanded));
  if (!path)
    return -1;

  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;

  size_t total = 0;

  while (total < (size_t)buffer_size - 1) {
    size_t got = fread(buffer + total, 1, (size_t)buffer_size - 1 - total, f);

    if (got == 0)
      break;

    total += got;
  }

  fclose(f);

  buffer[total] = '\0';
  return (long)total;
}

static char String_MatchingClose(char open) {
  switch (open) {
  case '(':
    return ')';
  case '[':
    return ']';
  case '{':
    return '}';
  case '<':
    return '>';
  case '"':
    return '"';
  case '\'':
    return '\'';
  case '`':
    return '`';
  default:
    return open;
  }
}

char *String_CutAt(const char *str, char delim) {
  if (!str)
    return NULL;
  const char *p = strchr(str, delim);
  if (!p)
    return NULL;
  return strdup(p);
}

char *String_CutBefore(const char *str, char delim) {
  if (!str)
    return NULL;
  const char *p = strchr(str, delim);
  if (!p)
    return strdup(str);
  size_t len = (size_t)(p - str);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, str, len);
  out[len] = '\0';
  return out;
}

char *String_CutAfter(const char *str, char delim) {
  if (!str)
    return NULL;
  const char *p = strchr(str, delim);
  if (!p)
    return NULL;
  return strdup(p + 1);
}

char *String_Between(const char *str, char open, char close) {
  if (!str)
    return NULL;
  const char *start = strchr(str, open);
  if (!start)
    return NULL;
  start++;
  if (open == close) {
    const char *end = strchr(start, close);
    if (!end)
      return NULL;
    size_t len = (size_t)(end - start);
    char *out = (char *)malloc(len + 1);
    if (!out)
      return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
  }
  int depth = 1;
  const char *cur = start;
  while (*cur) {
    if (*cur == open)
      depth++;
    else if (*cur == close) {
      depth--;
      if (depth == 0) {
        size_t len = (size_t)(cur - start);
        char *out = (char *)malloc(len + 1);
        if (!out)
          return NULL;
        memcpy(out, start, len);
        out[len] = '\0';
        return out;
      }
    }
    cur++;
  }
  return NULL;
}

char *String_Inside(const char *str, char delim) {
  if (!str)
    return NULL;
  char close = String_MatchingClose(delim);
  if (delim == close) {
    const char *p = str;
    char *last = NULL;
    while ((p = strchr(p, delim)) != NULL) {
      const char *start = p + 1;
      const char *end = strchr(start, close);
      if (!end)
        break;
      size_t len = (size_t)(end - start);
      char *cur = (char *)malloc(len + 1);
      if (!cur) {
        free(last);
        return NULL;
      }
      memcpy(cur, start, len);
      cur[len] = '\0';
      free(last);
      last = cur;
      p = end + 1;
    }
    return last;
  } else {
    const char *p = str;
    char *last = NULL;
    while ((p = strchr(p, delim)) != NULL) {
      const char *start = p + 1;
      int depth = 1;
      const char *cur = start;
      while (*cur) {
        if (*cur == delim)
          depth++;
        else if (*cur == close) {
          depth--;
          if (depth == 0)
            break;
        }
        cur++;
      }
      if (*cur != close)
        break;
      size_t len = (size_t)(cur - start);
      char *cur_out = (char *)malloc(len + 1);
      if (!cur_out) {
        free(last);
        return NULL;
      }
      memcpy(cur_out, start, len);
      cur_out[len] = '\0';
      free(last);
      last = cur_out;
      p = cur + 1;
    }
    return last;
  }
}

char *String_Trim(const char *str) {
  if (!str)
    return NULL;
  const char *s = str;
  while (*s && isspace((unsigned char)*s))
    s++;
  const char *e = str + strlen(str);
  while (e > s && isspace((unsigned char)*(e - 1)))
    e--;
  size_t len = (size_t)(e - s);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}

char *String_ToLower(const char *str) {
  if (!str)
    return NULL;
  char *out = strdup(str);
  if (!out)
    return NULL;
  for (char *p = out; *p; p++)
    *p = (char)tolower((unsigned char)*p);
  return out;
}

char *String_ToUpper(const char *str) {
  if (!str)
    return NULL;
  char *out = strdup(str);
  if (!out)
    return NULL;
  for (char *p = out; *p; p++)
    *p = (char)toupper((unsigned char)*p);
  return out;
}

bool String_StartsWith(const char *str, const char *prefix) {
  if (!str || !prefix)
    return false;
  size_t n = strlen(prefix);
  return strncmp(str, prefix, n) == 0;
}

bool String_EndsWith(const char *str, const char *suffix) {
  if (!str || !suffix)
    return false;
  size_t ls = strlen(str);
  size_t lx = strlen(suffix);
  if (lx > ls)
    return false;
  return strcmp(str + ls - lx, suffix) == 0;
}

char *String_Replace(const char *str, const char *from, const char *to) {
  if (!str || !from || !to)
    return NULL;
  size_t lf = strlen(from);
  if (lf == 0)
    return strdup(str);
  size_t lt = strlen(to);
  size_t count = 0;
  const char *p = str;
  while ((p = strstr(p, from))) {
    count++;
    p += lf;
  }
  size_t ls = strlen(str);
  size_t lo = ls + count * (lt - lf);
  char *out = (char *)malloc(lo + 1);
  if (!out)
    return NULL;
  char *dst = out;
  const char *src = str;
  while ((p = strstr(src, from))) {
    size_t n = (size_t)(p - src);
    memcpy(dst, src, n);
    dst += n;
    memcpy(dst, to, lt);
    dst += lt;
    src = p + lf;
  }
  size_t sl = strlen(src);
  memcpy(dst, src, sl + 1);
  return out;
}

char *String_Slice(const char *str, int start, int end) {
  if (!str)
    return NULL;
  int len = (int)strlen(str);
  if (start < 0)
    start = len + start;
  if (end < 0)
    end = len + end;
  if (start < 0)
    start = 0;
  if (end > len)
    end = len;
  if (end < start)
    end = start;
  size_t n = (size_t)(end - start);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, str + start, n);
  out[n] = '\0';
  return out;
}

StringList File_ReadLines(const char *path) {
  StringList result = {NULL, 0};

  if (!path)
    return result;

  char *content = read_all_dynamic(path);

  if (!content)
    return result;

  StringList all = String_Split(content, "\n");

  free(content);

  if (!all.items)
    return result;

  size_t kept = 0;

  for (size_t i = 0; i < all.count; i++) {
    char *line = all.items[i];

    if (!line)
      continue;

    line[strcspn(line, "\r")] = '\0';

    if (line[0] == '\0') {
      free(line);
      continue;
    }

    all.items[kept++] = line;
  }

  if (kept == 0) {
    free(all.items);
    return result;
  }

  result.items = all.items;
  result.count = kept;
  result.items[kept] = NULL;
  return result;
}

static const char *file_walk_builtin_ignored_names[] = {".git",
                                                        ".hg",
                                                        ".svn",
                                                        ".bzr",
                                                        ".fossil",
                                                        ".idea",
                                                        ".vscode",
                                                        ".cache",
                                                        ".local",
                                                        ".next",
                                                        ".nuxt",
                                                        ".output",
                                                        ".wrangler",
                                                        ".DS_Store",
                                                        "node_modules",
                                                        "__pycache__",
                                                        ".venv",
                                                        "venv",
                                                        "env",
                                                        ".env",
                                                        ".tox",
                                                        "target",
                                                        "build",
                                                        "dist",
                                                        "out",
                                                        "bin",
                                                        "obj",
                                                        "cmake-build-debug",
                                                        "cmake-build-release",
                                                        ".gradle",
                                                        ".mvn",
                                                        ".terraform",
                                                        ".stack-work",
                                                        ".cargo",
                                                        ".rustup",
                                                        "vendor",
                                                        NULL};

static bool file_walk_ignored_dir(const char *dir_name,
                                  char **names_to_ignore) {
  for (int i = 0; file_walk_builtin_ignored_names[i] != NULL; i++)
    if (Platform_StringEqualsCaseInsensitive(
            dir_name, file_walk_builtin_ignored_names[i]))
      return true;
  if (names_to_ignore) {
    for (int i = 0; names_to_ignore[i] != NULL; i++)
      if (Platform_StringEqualsCaseInsensitive(dir_name, names_to_ignore[i]))
        return true;
  }
  return false;
}

static bool file_walk_extension_allowed(const char *filename,
                                        char **allowed_extensions) {
  if (!allowed_extensions)
    return true;
  const char *ext = strrchr(filename, '.');
  if (!ext)
    return false;
  ext++;
  for (int i = 0; allowed_extensions[i] != NULL; i++) {
    const char *want = allowed_extensions[i];
    if (want && want[0] == '.')
      want++;
    if (Platform_StringEqualsCaseInsensitive(ext, want))
      return true;
  }
  return false;
}

static bool file_walk_push(ListOfFiles *list_of_files, const char *full_path,
                           const char *entry_name, bool strip_extension) {
  if (list_of_files->count >= list_of_files->capacity) {
    size_t new_capacity =
        list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;
    FileItem *new_items =
        realloc(list_of_files->files, new_capacity * sizeof(FileItem));
    if (!new_items)
      return false;
    list_of_files->files = new_items;
    list_of_files->capacity = new_capacity;
  }

  FileItem *item = &list_of_files->files[list_of_files->count];

  size_t plen = strlen(full_path);
  if (plen >= sizeof(item->path))
    plen = sizeof(item->path) - 1;
  memcpy(item->path, full_path, plen);
  item->path[plen] = '\0';

  char clean_name[256];
  size_t nlen = strlen(entry_name);
  if (nlen >= sizeof(clean_name))
    nlen = sizeof(clean_name) - 1;
  memcpy(clean_name, entry_name, nlen);
  clean_name[nlen] = '\0';

  if (strip_extension) {
    char *dot = strrchr(clean_name, '.');
    if (dot)
      *dot = '\0';
  }

  size_t clen = strlen(clean_name);
  if (clen >= sizeof(item->name))
    clen = sizeof(item->name) - 1;
  memcpy(item->name, clean_name, clen);
  item->name[clen] = '\0';

  list_of_files->count++;
  return true;
}

static bool file_walk_dir(const char *dir_path,
                          char **list_of_allowed_extensions,
                          ListOfFiles *list_of_files,
                          char **list_of_names_to_ignore, bool strip_extension,
                          int max_depth) {
  PlDirIter *it = Platform_DirOpen(dir_path);
  if (!it) {
    log_warn("file walk cannot open directory name='%s'", dir_path);
    return false;
  }

  char entry_name[512];
  bool is_dir = false;
  while (Platform_DirNext(it, entry_name, sizeof(entry_name), &is_dir)) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry_name);

    if (is_dir) {
      if (!file_walk_ignored_dir(entry_name, list_of_names_to_ignore) &&
          (max_depth == 0 || max_depth > 1)) {
        file_walk_dir(full_path, list_of_allowed_extensions, list_of_files,
                      list_of_names_to_ignore, strip_extension,
                      max_depth > 0 ? max_depth - 1 : 0);
      }
      continue;
    }

    if (!file_walk_extension_allowed(entry_name, list_of_allowed_extensions))
      continue;

    if (!file_walk_push(list_of_files, full_path, entry_name,
                        strip_extension)) {
      Platform_DirClose(it);
      return false;
    }
  }

  Platform_DirClose(it);
  return true;
}

bool FileWalk_AllFilesFromDir(const char *dir_path,
                              char **list_of_allowed_extensions,
                              ListOfFiles *list_of_files,
                              char **list_of_names_to_ignore,
                              bool strip_extension, int max_depth) {
  if (!dir_path || !list_of_files)
    return false;
  return file_walk_dir(dir_path, list_of_allowed_extensions, list_of_files,
                       list_of_names_to_ignore, strip_extension, max_depth);
}
