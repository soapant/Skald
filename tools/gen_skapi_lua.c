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
  const char *kind;
} RetEntry;

static RetEntry rets[512];
static int ret_count;

static const char *ret_for(const char *name) {
  for (int i = 0; i < ret_count; i++)
    if (strcmp(rets[i].name, name) == 0)
      return rets[i].kind;
  return "v";
}

static void collect_rets(char lines[MAX_LINES][LINE_SZ], int line_count) {
  for (int i = 0; i < line_count; i++) {
    char work[LINE_SZ];
    snprintf(work, sizeof(work), "%s", lines[i]);
    char *p = work;
    while (*p && isspace((unsigned char)*p))
      p++;
    char *semi = strrchr(p, ';');
    if (!semi)
      continue;
    *semi = '\0';
    char *open = strchr(p, '(');
    if (!open)
      continue;
    char *end = open;
    while (end > p && isspace((unsigned char)end[-1]))
      end--;
    char *start = end;
    while (start > p && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
      start--;
    size_t n = (size_t)(end - start);
    if (n == 0 || n >= NAME_SZ || ret_count >= 512)
      continue;
    char name[NAME_SZ];
    memcpy(name, start, n);
    name[n] = '\0';
    if (name[0] != 's' || name[1] != 'k')
      continue;
    snprintf(rets[ret_count].name, NAME_SZ, "%s", name);
    rets[ret_count].kind = ret_kind(p);
    ret_count++;
  }
}

typedef struct {
  char name[NAME_SZ];
  char sig[64];
} CbTypedef;

static CbTypedef cb_types[128];
static int cb_type_count;
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

typedef struct {
  char from[NAME_SZ];
  char to[NAME_SZ];
} Alias;

static Alias aliases[128];
static int alias_count;

static void parse_aliases(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line && alias_count < 128) {
    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    clean(work);
    if (strncmp(work, "typedef ", 8) == 0 && strchr(work, '{') == NULL &&
        strchr(work, '(') == NULL && strchr(work, '*') == NULL) {
      char *last = work + strlen(work);
      while (last > work && isspace((unsigned char)last[-1]))
        last--;
      char *start = last;
      while (start > work && (isalnum((unsigned char)start[-1]) ||
                              start[-1] == '_'))
        start--;
      char to[NAME_SZ] = {0};
      size_t n = (size_t)(last - start);
      if (n > 0 && n < NAME_SZ) {
        memcpy(to, start, n);
        to[n] = '\0';
        char *first = work + 8;
        while (*first == ' ')
          first++;
        char *end = first;
        while (*end && !isspace((unsigned char)*end))
          end++;
        char from[NAME_SZ] = {0};
        n = (size_t)(end - first);
        if (n > 0 && n < NAME_SZ &&
            strcmp(first, "struct") != 0 && strcmp(first, "enum") != 0 &&
            strcmp(first, "union") != 0) {
          memcpy(from, first, n);
          from[n] = '\0';
          if (from[0] && to[0] && strcmp(from, to) != 0) {
            snprintf(aliases[alias_count].from, NAME_SZ, "%s", to);
            snprintf(aliases[alias_count].to, NAME_SZ, "%s", from);
            alias_count++;
          }
        }
      }
    }
    line = strtok(NULL, "\n");
  }
  free(buf);
}

typedef struct {
  char name[NAME_SZ];
} BoolGlobal;

static BoolGlobal bool_globals[256];
static int bool_global_count;

static void parse_bool_globals(void) {
  char *buf = read_all("include/config_defs.h");
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line && bool_global_count < 256) {
    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    char *p = strstr(work, "X(bool,");
    if (p) {
      p += 7;
      while (*p == ' ')
        p++;
      char name[NAME_SZ] = {0};
      int ni = 0;
      while (*p && ni < NAME_SZ - 1 &&
             (isalnum((unsigned char)*p) || *p == '_'))
        name[ni++] = *p++;
      name[ni] = '\0';
      if (name[0]) {
        snprintf(bool_globals[bool_global_count++].name, NAME_SZ, "%s", name);
      }
    }
    line = strtok(NULL, "\n");
  }
  free(buf);
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

static void lua_expr(char *out, size_t cap, const char *expr) {
  char e[256];
  snprintf(e, sizeof(e), "%s", expr);
  char *v = trim(e);
  bool is_bool_global = false;
  for (int i = 0; i < bool_global_count && !is_bool_global; i++)
    if (strcmp(v, bool_globals[i].name) == 0)
      is_bool_global = true;
  if (is_bool_global)
    snprintf(out, cap, "native.global_b(\"%s\")", v);
  else if (strcmp(v, "true") == 0 || strcmp(v, "false") == 0 ||
           (v[0] >= '0' && v[0] <= '9') || v[0] == '"')
    snprintf(out, cap, "%s", v);
  else
    snprintf(out, cap, "nil");
}

static void emit_defaults(FILE *out, const char *rest) {
  const char *p = rest;
  while ((p = strstr(p, ".")) != NULL) {
    p++;
    char field[NAME_SZ] = {0};
    int fi = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_') && fi < NAME_SZ - 1)
      field[fi++] = *p++;
    field[fi] = '\0';
    if (!field[0] || !strchr(p, '='))
      continue;
    p = strchr(p, '=') + 1;
    const char *end = p;
    while (*end && *end != ',' && *end != ')' && *end != '}')
      end++;
    char expr[256] = {0};
    size_t n = (size_t)(end - p);
    if (n >= sizeof(expr))
      n = sizeof(expr) - 1;
    memcpy(expr, p, n);
    expr[n] = '\0';
    char value[256];
    lua_expr(value, sizeof(value), expr);
    fprintf(out, "if v.%s == nil then v.%s = %s end ", field, field, value);
    p = end;
  }
}

static void gen_wrapper(FILE *out, const char *name, const char *rest,
                        const char *params) {
  char raw[NAME_SZ] = {0};
  snprintf(raw, sizeof(raw), "ski_%s", name + 3);
  const char *kind = ret_for(raw);
  if (strcmp(kind, "v") != 0 && strcmp(kind, "i") != 0 &&
      strcmp(kind, "b") != 0 && strcmp(kind, "s") != 0)
    kind = "v";

  if (strstr(rest, "SK_STORE_NAME")) {
    fprintf(out,
            "M.%s = function(...) return native.%s(\"%s\", native.feature(), "
            "native.feature(), ...) end\n",
            name, kind, raw);
    return;
  }
  if (strstr(rest, "__FILE__")) {
    fprintf(out,
            "M.%s = function(...) return native.v(\"%s\", ..., native.owner()) "
            "end\n",
            name, raw);
    return;
  }
  if (strstr(rest, "NULL, NULL")) {
    fprintf(out, "M.%s = function(path) return native.image_size(path) end\n",
            name);
    return;
  }
  if (strstr(rest, "sk_cap_generation")) {
    fprintf(out,
            "M.%s = function(key) return function(...) return "
            "native.call_fn(\"sk_cap_resolve\", key, ...) end end\n",
            name);
    return;
  }
  if (strstr(rest, "(void *)") || strstr(rest, "(void*)")) {
    fprintf(out, "M.%s = function(key, fn) native.cap_register(key, fn) end\n",
            name);
    return;
  }
  if (strstr(name, "_fmt") || strstr(rest, "const SkMessageSpec")) {
    fprintf(out,
            "M.%s = function(t, ...) return native.struct_call(\"ski_message\", "
            "\"SkMessageSpec\", t or {}, \"v\") end\n",
            name);
    return;
  }

  char target[NAME_SZ] = {0};
  int ti = 0;
  const char *q = rest;
  while (*q && ti < NAME_SZ - 1 && (isalnum((unsigned char)*q) || *q == '_'))
    target[ti++] = *q++;
  target[ti] = '\0';

  if (target[0] == 's' && target[1] == 'k' && target[2] == '_' &&
      target[3] != 'i') {
    fprintf(out, "M.%s = function(t) local v = {} for k,x in pairs(t or {}) do "
                 "v[k] = x end ",
            name);
    emit_defaults(out, rest);
    fprintf(out, "return M.%s(v) end\n", target);
    return;
  }

  char *st = strstr(rest, "&(");
  if (st && target[0]) {
    st += 2;
    char stype[NAME_SZ] = {0};
    int si = 0;
    while (*st && si < NAME_SZ - 1 &&
           (isalnum((unsigned char)*st) || *st == '_'))
      stype[si++] = *st++;
    stype[si] = '\0';
    char lead[NAME_SZ] = {0};
    if (params) {
      const char *pp = params;
      while (*pp && *pp != '.') {
        if (isalnum((unsigned char)*pp) || *pp == '_') {
          size_t n = strlen(lead);
          if (n < NAME_SZ - 2) {
            lead[n] = *pp;
            lead[n + 1] = '\0';
          }
        } else if (*pp == ',' && lead[0]) {
          strcat(lead, ",");
        }
        pp++;
      }
    }
    fprintf(out, "M.%s = function(%st) local v = {} for k,x in pairs(t or {}) do "
                 "v[k] = x end ",
            name, lead[0] ? lead : "");
    emit_defaults(out, rest);
    fprintf(out, "return native.struct_call(\"%s\", \"%s\", v, \"%s\"", target,
            stype, kind);
    if (lead[0]) {
      char lead_args[NAME_SZ];
      snprintf(lead_args, sizeof(lead_args), "%s", lead);
      size_t ln = strlen(lead_args);
      if (ln && lead_args[ln - 1] == ',')
        lead_args[ln - 1] = '\0';
      fprintf(out, ",%s", lead_args);
    }
    fprintf(out, ") end\n");
    return;
  }

  fprintf(out, "M.%s = M.%s or function(...) return native.%s(\"%s\", ...) "
               "end\n",
          name, target[0] ? target : name, kind, name);
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

  for (size_t h = 0; h < sizeof(headers) / sizeof(headers[0]); h++)
    parse_aliases(headers[h]);

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
          char aux[160] = "0";
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
                if (ck) {
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
              if (ck) {
                kind = LSF_CB;
                cb_kind = strcmp(ck, "LCB_V") == 0     ? 0
                          : strcmp(ck, "LCB_PS") == 0  ? 1
                          : strcmp(ck, "LCB_BPS") == 0 ? 2
                                                       : 3;
              }
              is_cb_type = true;
            }
            if (strchr(decl, '[')) {
              kind = LSF_STR_ARRAY;
              snprintf(aux, sizeof(aux), "%d", atoi(strchr(decl, '[') + 1));
            }
            else if (is_cb_type)
              ;
            else if (strcmp(field, "buffer") == 0 &&
                     (strstr(decl, "char *") || strstr(decl, "char*"))) {
              kind = LSF_BUFFER;
              snprintf(aux, sizeof(aux), "offsetof(%s, buffer_size)", name);
            } else if (strstr(decl, "char *") || strstr(decl, "char*") ||
                       strstr(decl, "const char"))
              kind = LSF_STR;
            else if (strchr(decl, '*'))
              kind = LSF_PTR;
            else if (strstr(decl, "bool") == decl)
              kind = LSF_BOOL;
            else if (strstr(decl, "float") == decl)
              kind = LSF_FLOAT;
            else if (strstr(decl, "double") == decl)
              kind = LSF_DOUBLE;
            else if (strstr(decl, "int") || strstr(decl, "unsigned") ||
                     strstr(decl, "long") || strstr(decl, "ColorRef") ||
                     strstr(decl, "MessageSlot") || strstr(decl, "size_t") ||
                     strstr(decl, "Timer"))
              kind = LSF_INT;
            else
              kind = LSF_SKIP;
          }
          if (field[0] && kind != LSF_SKIP)
            fprintf(out, "    {\"%s\", %s, offsetof(%s, %s), %s, %d},\n", field,
                    kind == LSF_INT      ? "LSF_INT"
                    : kind == LSF_BOOL   ? "LSF_BOOL"
                    : kind == LSF_FLOAT  ? "LSF_FLOAT"
                    : kind == LSF_DOUBLE ? "LSF_DOUBLE"
                    : kind == LSF_STR    ? "LSF_STR"
                    : kind == LSF_STR_ARRAY ? "LSF_STR_ARRAY"
                    : kind == LSF_BUFFER ? "LSF_BUFFER"
                    : kind == LSF_PTR    ? "LSF_PTR"
                                         : "LSF_CB",
                    name, field, aux, cb_kind);
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
  fprintf(out, "};\n");
  fprintf(out, "static const char *lua_alias_from[] = {\n");
  for (int i = 0; i < alias_count; i++)
    fprintf(out, "    \"%s\",\n", aliases[i].from);
  fprintf(out, "};\nstatic const char *lua_alias_to[] = {\n");
  for (int i = 0; i < alias_count; i++)
    fprintf(out, "    \"%s\",\n", aliases[i].to);
  fprintf(out,
          "};\n"
          "static const LuaStructDesc *ski_lua_struct_find(const char *name) {\n"
          "  for (size_t i = 0; i < "
          "sizeof(lua_alias_from)/sizeof(lua_alias_from[0]); i++)\n"
          "    if (strcmp(name, lua_alias_from[i]) == 0)\n"
          "      name = lua_alias_to[i];\n"
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
  collect_rets(lines, line_count);
  parse_bool_globals();

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
      if (!name[0] || name[0] != 's' || name[1] != 'k')
        continue;
      char params[NAME_SZ] = {0};
      if (*p == '(') {
        char *close_paren = strchr(p, ')');
        if (close_paren) {
          size_t pn = (size_t)(close_paren - p + 1);
          if (pn < NAME_SZ - 1) {
            memcpy(params, p, pn);
            params[pn] = '\0';
          }
          p = close_paren + 1;
        }
      }
      char *rest = trim(p);
      gen_wrapper(out, name, rest, params);
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
    if (!fname[0] || fname[0] != 's' || fname[1] != 'k')
      continue;
    if (strstr(p, "typedef"))
      continue;
    if (strstr(fname, "_fmt")) {
      gen_wrapper(out, fname, "const SkMessageSpec", "");
    } else {
      fprintf(out, "function M.%s(...) return native.%s(\"%s\", ...) end\n",
              fname, ret_kind(p), fname);
    }
  }

  fprintf(out, "\nreturn M\n");
  fclose(out);
  return gen_struct_header();
}
