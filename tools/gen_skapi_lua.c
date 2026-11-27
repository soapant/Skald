#include "core/lua_struct.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
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
    {"sk_cap_bind",
     "M.sk_cap_bind = function(key) return function(...) return "
     "native.call_fn(\"sk_cap_resolve\", key, ...) end end"},
    {"sk_message_fmt",
     "M.sk_message_fmt = function(t, ...) return native.struct_call(\"ski_message\", "
     "\"SkMessageSpec\", t or {}, \"v\") end"},
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
     "native.struct_call(\"ski_window\", \"PromptSpec\", v, \"v\") end"},
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

typedef struct {
  char name[NAME_SZ];
  char sig[64];
} CbTypedef;

static CbTypedef cb_types[128];
static int cb_type_count;
static bool skip_field_name(const char *name) {
  static const char *skip[] = {
      "hovered",   "was_down",  "clicked",   "focused",    "render_data",
      "ud",        "cb_ud",     "buffer",    "buffer_size", "items",
      "lb",        "font",      "mw",        "mh",
      "prompt_gap", "caret_x",  "caret_y",   "caret_pos",  "text_y",
      "fps",       "anchor_y_to_prompt_h", "auto_focus", "hide_caret",
      "whitelist", "blacklist", "frame",     "pad",        "created_at",
      "ticks",     "out_index", "out"};
  for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); i++)
    if (strcmp(name, skip[i]) == 0)
      return true;
  return false;
}

static char *read_all(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    fclose(f);
    return NULL;
  }
  buf[n] = '\0';
  fclose(f);
  return buf;
}

static void clean(char *s) {
  for (char *p = s; *p; p++)
    if (*p == '\n' || *p == '\r' || *p == '\t')
      *p = ' ';
  char *d = s;
  while (*d == ' ')
    d++;
  if (d != s)
    memmove(s, d, strlen(d) + 1);
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == ';' || s[n - 1] == ','))
    s[--n] = '\0';
}

static const char *cb_kind_for_sig(const char *sig) {
  if (strcmp(sig, "void()") == 0)
    return "LCB_V";
  if (strcmp(sig, "bool()") == 0)
    return "LCB_B";
  if (strcmp(sig, "void(p)") == 0)
    return "LCB_PS";
  if (strcmp(sig, "bool(p)") == 0)
    return "LCB_BPS";
  return NULL;
}

static void normalize_sig(char *out, size_t cap, const char *ret,
                          const char *args) {
  char r[32] = {0};
  size_t i = 0;
  while (*ret && i < sizeof(r) - 1 && !isspace((unsigned char)*ret))
    r[i++] = *ret++;
  r[i] = '\0';
  char a[128] = {0};
  char *dst = a;
  const char *p = args;
  while (*p && (size_t)(dst - a) < sizeof(a) - 2) {
    if (strncmp(p, "void", 4) == 0 && p[4] == ')')
      break;
    if (strncmp(p, "PromptState", 11) == 0 ||
        strncmp(p, "void *", 6) == 0 || strncmp(p, "void*", 5) == 0) {
      *dst++ = 'p';
    } else if (strncmp(p, "const char", 10) == 0 ||
               strncmp(p, "char *", 6) == 0) {
      *dst++ = 's';
    } else if (strncmp(p, "int", 3) == 0) {
      *dst++ = 'i';
    }
    while (*p && *p != ',')
      p++;
    if (*p == ',') {
      *dst++ = ',';
      p++;
    } else {
      break;
    }
  }
  *dst = '\0';
  snprintf(out, cap, "%s(%s)", r, a);
}

static void parse_callback_typedefs(const char *src) {
  char *buf = read_all(src);
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line) {
    char *p = line;
    clean(p);
    char *lp = strstr(p, "(*");
    if (strncmp(p, "typedef", 7) == 0 && lp) {
      char name[NAME_SZ] = {0};
      int ni = 0;
      char *q = lp + 2;
      while (*q && ni < NAME_SZ - 1 && *q != ')')
        name[ni++] = *q++;
      name[ni] = '\0';
      char *args = strstr(q, ")(");
      if (args && cb_type_count < 128) {
        args += 2;
        char *close = strchr(args, ')');
        if (close) {
          *close = '\0';
          char ret[64] = {0};
          size_t n = (size_t)(lp - p);
          if (n > sizeof(ret) - 1)
            n = sizeof(ret) - 1;
          memcpy(ret, p, n);
          ret[n] = '\0';
          clean(ret);
          if (strncmp(ret, "typedef ", 8) == 0)
            memmove(ret, ret + 8, strlen(ret) - 7);
          CbTypedef *t = &cb_types[cb_type_count++];
          snprintf(t->name, sizeof(t->name), "%s", name);
          normalize_sig(t->sig, sizeof(t->sig), ret, args);
        }
      }
    }
    line = strtok(NULL, "\n");
  }
  free(buf);
}

static int gen_struct_header(void) {
  const char *headers[] = {
      "include/core/skapi.h",
      "include/core/types/prompt.h",
      "include/core/types/window.h",
      "include/core/types/skapi_types.h",
      "include/core/checkbox.h",
      "include/core/cc_registry.h",
  };

  parse_callback_typedefs("include/core/types/prompt.h");
  parse_callback_typedefs("include/core/skapi.h");
  FILE *out = fopen("include/core/skapi_lua_generated.h", "w");
  if (!out) {
    fprintf(stderr, "gen_skapi_lua: cannot write generated struct header\n");
    return 1;
  }
  fprintf(out,
          "#ifndef SKAPI_LUA_GENERATED_H\n"
          "#define SKAPI_LUA_GENERATED_H\n\n"
          "#include \"core/skapi.h\"\n"
          "#include \"core/lua_struct.h\"\n"
          "#include <string.h>\n\n");

  for (size_t h = 0; h < sizeof(headers) / sizeof(headers[0]); h++) {
    char *buf = read_all(headers[h]);
    if (!buf) {
      fprintf(stderr, "cannot read %s\n", headers[h]);
      continue;
    }
    char *p = buf;
    while ((p = strstr(p, "typedef struct {")) != NULL) {
      char *open = strchr(p, '{');
      char *close = strchr(open, '}');
      if (!close)
        break;
      char *after = strchr(close, ';');
      if (!after)
        break;
      char name[NAME_SZ] = {0};
      char *np = close + 1;
      while (*np && isspace((unsigned char)*np))
        np++;
      int name_i = 0;
      while (*np && (isalnum((unsigned char)*np) || *np == '_')) {
        if (name_i < NAME_SZ - 1)
          name[name_i++] = *np;
        np++;
      }
      name[name_i] = '\0';
      if (name[0]) {
        fprintf(out, "static const LuaFieldDesc lf_%s[] = {\n", name);
        char *body = open + 1;
        char *end = close;
        while (body < end) {
          char *semi = strchr(body, ';');
          if (!semi || semi > end)
            break;
          char decl[512] = {0};
          size_t n = (size_t)(semi - body);
          if (n > sizeof(decl) - 1)
            n = sizeof(decl) - 1;
          memcpy(decl, body, n);
          decl[n] = '\0';
          clean(decl);
          body = semi + 1;

          char field[NAME_SZ] = {0};
          LuaFieldKind kind = LSF_SKIP;
          int cb_kind = 0;
          char *fptr = strstr(decl, "(*");
          if (fptr) {
            int fi = 0;
            char *q = fptr + 2;
            while (*q && fi < NAME_SZ - 1 && *q != ')')
              field[fi++] = *q++;
            field[fi] = '\0';
            char *args = strstr(q, ")(");
            if (args && (strstr(decl, "void") == decl ||
                         strncmp(decl, "bool", 4) == 0)) {
              args += 2;
              char *cl = strchr(args, ')');
              if (cl) {
                *cl = '\0';
                char ret[64] = {0};
                size_t rn = (size_t)(fptr - decl);
                if (rn > sizeof(ret) - 1)
                  rn = sizeof(ret) - 1;
                memcpy(ret, decl, rn);
                ret[rn] = '\0';
                clean(ret);
                char sig[64];
                normalize_sig(sig, sizeof(sig), ret, args);
                const char *ck = cb_kind_for_sig(sig);
                if (ck && !skip_field_name(field)) {
                  kind = LSF_CB;
                  if (strcmp(ck, "LCB_V") == 0)
                    cb_kind = 0;
                  else if (strcmp(ck, "LCB_PS") == 0)
                    cb_kind = 1;
                  else if (strcmp(ck, "LCB_BPS") == 0)
                    cb_kind = 2;
                  else
                    cb_kind = 3;
                }
              }
            }
          } else {
            char *last = NULL;
            for (char *q = decl; *q; q++)
              if (isspace((unsigned char)*q) || *q == '*')
                last = q;
            if (last) {
              int fi = 0;
              char *q = last + 1;
              while (*q && fi < NAME_SZ - 1 &&
                     (isalnum((unsigned char)*q) || *q == '_'))
                field[fi++] = *q++;
              field[fi] = '\0';
            }
            bool is_cb_type = false;
            for (int t = 0; t < cb_type_count && !is_cb_type; t++) {
              char *found = strstr(decl, cb_types[t].name);
              if (!found)
                continue;
              char next_char = found[strlen(cb_types[t].name)];
              if (next_char != '\0' && next_char != ' ' && next_char != '*')
                continue;
              const char *ck = cb_kind_for_sig(cb_types[t].sig);
              if (ck && !skip_field_name(field)) {
                kind = LSF_CB;
                cb_kind = strcmp(ck, "LCB_V") == 0     ? 0
                          : strcmp(ck, "LCB_PS") == 0  ? 1
                          : strcmp(ck, "LCB_BPS") == 0 ? 2
                                                       : 3;
              }
              is_cb_type = true;
            }
            if (strstr(decl, "[") || skip_field_name(field))
              kind = LSF_SKIP;
            else if (is_cb_type)
              ;
            else if (strstr(decl, "bool") == decl)
              kind = LSF_BOOL;
            else if (strstr(decl, "float") == decl)
              kind = LSF_FLOAT;
            else if (strstr(decl, "double") == decl)
              kind = LSF_DOUBLE;
            else if (strstr(decl, "char *") || strstr(decl, "char*") ||
                     strstr(decl, "const char"))
              kind = LSF_STR;
            else if (strstr(decl, "void") == decl && strstr(decl, "*"))
              kind = LSF_SKIP;
            else if (strstr(decl, "int") || strstr(decl, "unsigned") ||
                     strstr(decl, "long") || strstr(decl, "ColorRef") ||
                     strstr(decl, "MessageSlot") || strstr(decl, "size_t"))
              kind = LSF_INT;
            else
              kind = LSF_SKIP;
          }
          if (field[0] && kind != LSF_SKIP)
            fprintf(out, "    {\"%s\", %s, offsetof(%s, %s), %d},\n", field,
                    kind == LSF_INT      ? "LSF_INT"
                    : kind == LSF_BOOL   ? "LSF_BOOL"
                    : kind == LSF_FLOAT  ? "LSF_FLOAT"
                    : kind == LSF_DOUBLE ? "LSF_DOUBLE"
                    : kind == LSF_STR    ? "LSF_STR"
                                         : "LSF_CB",
                    name, field, cb_kind);
        }
        fprintf(out, "};\n");
        fprintf(out,
                "static const LuaStructDesc lsd_%s = {\"%s\", sizeof(%s), "
                "lf_%s, (int)(sizeof(lf_%s)/sizeof(lf_%s[0]))};\n",
                name, name, name, name, name, name);
      }
      p = np;
    }
    free(buf);
  }

  fprintf(out, "static const LuaStructDesc *lua_generated_structs[] = {\n");
  fprintf(out, "    &lsd_CcEntrySpec, &lsd_SkHotkeySpec, &lsd_PromptSpec,\n");
  fprintf(out, "    &lsd_SkTextSpec, &lsd_DrawRect,\n");
  fprintf(out, "    &lsd_CheckboxSpec, &lsd_SkMessageSpec, &lsd_SkEditSpec,\n");
  fprintf(out, "    &lsd_ListSpec,\n");
  fprintf(out,
          "};\n"
          "static const LuaStructDesc *ski_lua_struct_find(const char *name) {\n"
          "  if (strcmp(name, \"SkCcEntrySpec\") == 0) name = \"CcEntrySpec\";\n"
          "  if (strcmp(name, \"SkCheckboxSpec\") == 0) name = \"CheckboxSpec\";\n"
          "  if (strcmp(name, \"SkPromptSpec\") == 0) name = \"PromptSpec\";\n"
          "  if (strcmp(name, \"SkRect\") == 0) name = \"DrawRect\";\n"
          "  for (size_t i = 0; i < "
          "sizeof(lua_generated_structs)/sizeof(lua_generated_structs[0]); "
          "i++)\n"
          "    if (strcmp(name, lua_generated_structs[i]->name) == 0)\n"
          "      return lua_generated_structs[i];\n"
          "  return NULL;\n"
          "}\n\n"
          "#endif\n");
  fclose(out);
  return 0;
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
  return gen_struct_header();
}
