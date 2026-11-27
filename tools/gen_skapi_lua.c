#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 4096
#define LINE_SZ 4096
#define NAME_SZ 128

typedef struct {
  const char *name;
  const char *code;
} Override;

static const Override overrides[] = {
    {"sk_store_set",
     "M.sk_store_set = function(value) native.v(\"ski_store_set\", "
     "native.feature(), native.feature(), value) end"},
    {"sk_store_append",
     "M.sk_store_append = function(value) native.v(\"ski_store_append\", "
     "native.feature(), native.feature(), value) end"},
    {"sk_store_get_all",
     "M.sk_store_get_all = function() return native.s(\"ski_store_get_all\", "
     "native.feature(), native.feature()) end"},
    {"sk_store_get_last",
     "M.sk_store_get_last = function() return native.s(\"ski_store_get_last\", "
     "native.feature(), native.feature()) end"},
    {"sk_store_replace_all",
     "M.sk_store_replace_all = function(values, count) "
     "native.v(\"ski_store_replace_all\", native.feature(), native.feature(), "
     "values, count) end"},
    {"sk_store_clear",
     "M.sk_store_clear = function() native.v(\"ski_store_clear\", "
     "native.feature(), native.feature()) end"},
    {"sk_add_dep",
     "M.sk_add_dep = function(dep) native.v(\"ski_add_dep\", dep, "
     "native.owner()) end"},
    {"sk_get_image_size",
     "M.sk_get_image_size = function(path) return native.image_size(path) end"},
    {"sk_set_window_geometry",
     "M.sk_set_window_geometry = function(window, t) return "
     "native.struct_call(\"ski_set_window_geometry\", \"SkRect\", t or {}, "
     "\"b\", window) end"},
    {"sk_populate_listbox",
     "M.sk_populate_listbox = function(t) return "
     "native.struct_call(\"ski_populate_listbox\", \"SkPopulateListboxSpec\", "
     "t or {}, \"v\") end"},
    {"sk_cap_register",
     "M.sk_cap_register = function(key, fn) native.cap_register(key, fn) end"},
    {"sk_cap_bind", "M.sk_cap_bind = function(key) return native.cap_bind(key) end"},
    {"sk_message_fmt",
     "M.sk_message_fmt = function(t, ...) native.message_fmt(t or {}, ...) end"},
    {"sk_overlay",
     "M.sk_overlay = function(t) local v = {} for k,x in pairs(t or {}) do "
     "v[k] = x end v.timeout = v.timeout or 10000000.0 return "
     "native.struct_call(\"ski_message\", \"SkMessageSpec\", v, \"v\") end"},
    {"sk_draw_rect",
     "M.sk_draw_rect = function(t) local v = {} for k,x in pairs(t or {}) do "
     "v[k] = x end if v.border == nil then v.border = "
     "native.global_b(\"EnableBorders\") end return "
     "native.struct_call(\"ski_draw_rect\", \"DrawRect\", v, \"b\") end"},
    {"sk_add_feature",
     "M.sk_add_feature = function(t) local v = {} for k,x in pairs(t or {}) "
     "do v[k] = x end v.type = v.type or \"Feature\" return "
     "native.struct_call(\"ski_add_to_cc\", \"SkCcEntrySpec\", v, \"b\") end"},
    {"sk_subwindow",
     "M.sk_subwindow = function(t) local v = {} for k,x in pairs(t or {}) do "
     "v[k] = x end v.subwindow = true return "
     "native.struct_call(\"ski_window\", \"SkPromptSpec\", v, \"v\") end"},
    {"sk_place_button",
     "M.sk_place_button = function(t) local v = {} for k,x in pairs(t or {}) "
     "do v[k] = x end v.border = true v.button = true return "
     "native.struct_call(\"ski_place_checkbox\", \"SkCheckboxSpec\", v, \"b\") "
     "end"},
    {"sk_place_checkbox",
     "M.sk_place_checkbox = function(t) local v = {} for k,x in pairs(t or {}) "
     "do v[k] = x end v.border = true v.checkbox = true return "
     "native.struct_call(\"ski_place_checkbox\", \"SkCheckboxSpec\", v, \"b\") "
     "end"},
};

static bool overridden(const char *name) {
  for (size_t i = 0; i < sizeof(overrides) / sizeof(overrides[0]); i++)
    if (strcmp(overrides[i].name, name) == 0)
      return true;
  return false;
}

static const char *ret_kind(const char *line) {
  if (strncmp(line, "bool", 4) == 0)
    return "b";
  if (strncmp(line, "char", 4) == 0)
    return "s";
  if (strncmp(line, "float", 5) == 0 || strncmp(line, "double", 6) == 0)
    return "f";
  if (strncmp(line, "void", 4) == 0)
    return "v";
  return "i";
}

static char *trim(char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1]))
    s[--n] = '\0';
  return s;
}

int main(void) {
  FILE *in = fopen("include/core/skapi.h", "r");
  if (!in) {
    fprintf(stderr, "gen_skapi_lua: cannot open include/core/skapi.h\n");
    return 1;
  }
  FILE *out = fopen("src/skapi.lua", "w");
  if (!out) {
    fclose(in);
    fprintf(stderr, "gen_skapi_lua: cannot write src/skapi.lua\n");
    return 1;
  }

  static char lines[MAX_LINES][LINE_SZ];
  int line_count = 0;
  int cur = -1;
  char buf[LINE_SZ];
  while (fgets(buf, sizeof(buf), in)) {
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
      buf[--n] = '\0';
    bool cont = n && buf[n - 1] == '\\';
    if (cont)
      buf[n - 1] = '\0';
    if (cur < 0) {
      if (line_count >= MAX_LINES)
        break;
      cur = line_count++;
      snprintf(lines[cur], LINE_SZ, "%s", buf);
    } else {
      size_t used = strlen(lines[cur]);
      snprintf(lines[cur] + used, LINE_SZ - used, "%s", buf);
    }
    if (!cont)
      cur = -1;
  }
  fclose(in);

  fprintf(out,
          "-- Generated by tools/gen_skapi_lua.c. Do not edit.\n"
          "local native = require(\"skapi_native\")\n"
          "local M = {}\n\n");

  for (int i = 0; i < line_count; i++) {
    char *p = lines[i];
    while (*p && isspace((unsigned char)*p))
      p++;

    if (strncmp(p, "#define ", 8) == 0) {
      p += 8;
      char name[NAME_SZ] = {0};
      int ni = 0;
      while (*p && ni < NAME_SZ - 1 &&
             (isalnum((unsigned char)*p) || *p == '_'))
        name[ni++] = *p++;
      name[ni] = '\0';
      if (!name[0] || name[0] != 's' || name[1] != 'k' || overridden(name))
        continue;
      char *rest = trim(p);
      if (*rest == '(') {
        char *close = strchr(rest, ')');
        if (close)
          rest = close + 1;
        rest = trim(rest);
      }

      char *lp = strchr(name, '(');
      if (lp)
        *lp = '\0';
      char raw[NAME_SZ] = {0};
      int ri = 0;
      while (*rest && ri < NAME_SZ - 1 &&
             (isalnum((unsigned char)*rest) || *rest == '_'))
        raw[ri++] = *rest++;
      raw[ri] = '\0';

      char *st = strstr(rest, "&(");
      if (st && raw[0]) {
        st += 2;
        char stype[NAME_SZ] = {0};
        int si = 0;
        while (*st && si < NAME_SZ - 1 &&
               (isalnum((unsigned char)*st) || *st == '_'))
          stype[si++] = *st++;
        stype[si] = '\0';
        if (stype[0])
          fprintf(out,
                  "M.%s = function(t) return native.struct_call(\"%s\", "
                  "\"%s\", t or {}, \"v\") end\n",
                  name, raw, stype);
      } else if (raw[0]) {
        fprintf(out,
                "M.%s = M.%s or function(...) return native.v(\"%s\", "
                "...) end\n",
                name, raw, name);
      }
      continue;
    }

    char *semi = strrchr(p, ';');
    if (!semi)
      continue;
    *semi = '\0';
    char fname[NAME_SZ] = {0};
    char *open = strchr(p, '(');
    if (!open)
      continue;
    char *end = open;
    while (end > p && isspace((unsigned char)end[-1]))
      end--;
    char *start = end;
    while (start > p && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
      start--;
    size_t flen = (size_t)(end - start);
    if (flen == 0 || flen >= NAME_SZ)
      continue;
    memcpy(fname, start, flen);
    fname[flen] = '\0';
    if (!fname[0] || fname[0] != 's' || fname[1] != 'k' || overridden(fname))
      continue;
    if (strstr(p, "typedef"))
      continue;
    fprintf(out, "function M.%s(...) return native.%s(\"%s\", ...) end\n",
            fname, ret_kind(p), fname);
  }

  for (size_t i = 0; i < sizeof(overrides) / sizeof(overrides[0]); i++)
    fprintf(out, "%s\n", overrides[i].code);

  fprintf(out, "\nreturn M\n");
  fclose(out);
  return 0;
}
