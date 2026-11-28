#include "core/lua_struct.h"
#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 16384
#define LINE_SZ 8192
#define NAME_SZ 128

static char g_out_dir[768] = "";

static char *out_path(const char *rel) {
  static char bufs[4][1024];
  static int slot;
  slot = (slot + 1) % 4;
  snprintf(bufs[slot], sizeof(bufs[slot]), "%s%s%s", g_out_dir,
           g_out_dir[0] ? "/" : "", rel);
  return bufs[slot];
}

static void clean(char *s);
static const char *resolve_alias_base(const char *t);
static bool is_scalar_type(const char *t);
static bool is_builtin_scalar(const char *t);
static bool is_struct_type(const char *t);

static int gen_errors;

static void gen_error(const char *message) {
  fprintf(stderr, "gen_skapi_lua: %s\n", message);
  gen_errors++;
}

static const char *ret_kind(const char *line) {
  const char *b = resolve_alias_base(line);
  if (strncmp(b, "bool", 4) == 0)
    return "b";
  if (strncmp(b, "const char", 10) == 0 ||
      (strncmp(b, "char", 4) == 0 && strchr(b, '*') != NULL))
    return "s";
  if (strncmp(b, "char", 4) == 0)
    return "i";
  if (strncmp(b, "float", 5) == 0 || strncmp(b, "double", 6) == 0)
    return "f";
  if (strncmp(b, "void", 4) == 0)
    return strchr(b, '*') != NULL ? "p" : "v";
  const char *space = strchr(b, ' ');
  size_t n = space ? (size_t)(space - b) : strlen(b);
  if (memchr(b, '*', n) != NULL)
    return "p";
  if (is_builtin_scalar(b))
    return "i";
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
  char type[256];
  const char *kind;
  bool found;
} RetEntry;

typedef struct {
  char name[NAME_SZ];
  char ret[NAME_SZ];
  char params[1024];
  bool varargs;
} FuncDesc;

static RetEntry rets[4096];
static int ret_count;
static FuncDesc funcs[4096];
static int func_count;
static char struct_names[2048][NAME_SZ];
static char struct_sources[2048][512];
static int struct_name_count;

static const char *ret_for(const char *name) {
  for (int i = 0; i < ret_count; i++)
    if (strcmp(rets[i].name, name) == 0)
      return rets[i].kind;
  return "v";
}

static void collect_rets(char lines[][LINE_SZ], int line_count) {
  for (int i = 0; i < line_count; i++) {
    char work[LINE_SZ];
    snprintf(work, sizeof(work), "%s", lines[i]);
    char *p = work;
    while (*p && isspace((unsigned char)*p))
      p++;
    if (*p == '#')
      continue;
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
    if (n == 0 || n >= NAME_SZ || ret_count >= 4096)
      continue;
    char name[NAME_SZ];
    memcpy(name, start, n);
    name[n] = '\0';
    if (name[0] != 's' || name[1] != 'k')
      continue;
    snprintf(rets[ret_count].name, NAME_SZ, "%s", name);
    size_t tn2 = (size_t)(open - p);
    if (tn2 >= sizeof(rets[ret_count].type))
      tn2 = sizeof(rets[ret_count].type) - 1;
    memcpy(rets[ret_count].type, p, tn2);
    rets[ret_count].type[tn2] = '\0';
    clean(rets[ret_count].type);
    rets[ret_count].kind = ret_kind(p);
    rets[ret_count].found = true;
    ret_count++;

    if (func_count < 4096) {
      FuncDesc *fd = &funcs[func_count++];
      snprintf(fd->name, NAME_SZ, "%s", name);
      size_t rn = (size_t)(start - p);
      if (rn >= NAME_SZ)
        rn = NAME_SZ - 1;
      memcpy(fd->ret, p, rn);
      fd->ret[rn] = '\0';
      clean(fd->ret);
      int depth = 0;
      char *close_paren = NULL;
      for (char *q = open; *q; q++) {
        if (*q == '(')
          depth++;
        else if (*q == ')') {
          depth--;
          if (depth == 0) {
            close_paren = q;
            break;
          }
        }
      }
      if (close_paren) {
        size_t pn = (size_t)(close_paren - open - 1);
        if (pn >= sizeof(fd->params))
          pn = sizeof(fd->params) - 1;
        memcpy(fd->params, open + 1, pn);
        fd->params[pn] = '\0';
        clean(fd->params);
        fd->varargs = strstr(fd->params, "...") != NULL;
      }
    }
  }
}

typedef struct {
  char name[NAME_SZ];
  char sig[64];
  char ptr_type[256];
} CbTypedef;

static CbTypedef cb_types[1024];
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

static void write_if_changed(const char *final, const char *tmp) {
  char *old = read_all(final);
  char *now = read_all(tmp);
  if (old && now && strcmp(old, now) == 0) {
    free(old);
    free(now);
    remove(tmp);
    return;
  }
  free(old);
  free(now);
  remove(final);
  if (rename(tmp, final) != 0)
    fprintf(stderr, "gen_skapi_lua: cannot replace %s\\n", final);
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

static Alias aliases[1024];
static int alias_count;
static char scalar_names[1024][NAME_SZ];
static int scalar_name_count;
static char handle_names[256][NAME_SZ];
static int handle_name_count;

typedef struct {
  char name[NAME_SZ];
  long long value;
} EnumConst;

static EnumConst enum_consts[2048];
static int enum_count;
static char define_names[2048][NAME_SZ];
static char define_exprs[2048][256];
static bool define_resolved[2048];
static int define_count;

static void parse_aliases(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line && alias_count < 1024) {
    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    clean(work);
    if (strncmp(work, "typedef ", 8) == 0 && strchr(work, '{') == NULL &&
        strchr(work, '(') == NULL) {
      char *last = work + strlen(work);
      while (last > work && isspace((unsigned char)last[-1]))
        last--;
      char *start = last;
      while (start > work &&
             (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
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
        if (n > 0 && n < NAME_SZ) {
          memcpy(from, first, n);
          from[n] = '\0';
          if (strcmp(from, "struct") != 0 && strcmp(from, "enum") != 0 &&
              strcmp(from, "union") != 0 && from[0] && to[0] &&
              strcmp(from, to) != 0) {
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

static void parse_enum_typedefs(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *p = buf;
  while ((p = strstr(p, "typedef enum")) != NULL && scalar_name_count < 1024) {
    char *q = p + strlen("typedef enum");
    while (*q && isspace((unsigned char)*q))
      q++;
    char name[NAME_SZ] = {0};
    if (*q == '{') {
      char *close = strchr(q, '}');
      if (!close)
        break;
      char *semi = strchr(close, ';');
      if (!semi)
        break;
      char *n = close + 1;
      while (n < semi && isspace((unsigned char)*n))
        n++;
      int ni = 0;
      while (n < semi && ni < NAME_SZ - 1 &&
             (isalnum((unsigned char)*n) || *n == '_'))
        name[ni++] = *n++;
      name[ni] = '\0';
      p = semi + 1;
    } else {
      int ni = 0;
      while (*q && ni < NAME_SZ - 1 &&
             (isalnum((unsigned char)*q) || *q == '_'))
        name[ni++] = *q++;
      name[ni] = '\0';
      p = q;
    }
    if (name[0]) {
      bool known = false;
      for (int i = 0; i < scalar_name_count && !known; i++)
        known = strcmp(scalar_names[i], name) == 0;
      if (!known)
        snprintf(scalar_names[scalar_name_count++], NAME_SZ, "%s", name);
    }
  }
  free(buf);
}

static void add_enum_const(const char *name, long long value) {
  if (!name[0])
    return;
  for (int i = 0; i < enum_count; i++)
    if (strcmp(enum_consts[i].name, name) == 0)
      return;
  if (enum_count < 2048) {
    snprintf(enum_consts[enum_count].name, NAME_SZ, "%s", name);
    enum_consts[enum_count++].value = value;
  }
}

static void parse_enum_values(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *p = buf;
  while ((p = strstr(p, "enum")) != NULL && enum_count < 2048) {
    if ((p > buf && (isalnum((unsigned char)p[-1]) || p[-1] == '_'))) {
      p += 4;
      continue;
    }
    char *q = p + 4;
    while (*q && isspace((unsigned char)*q))
      q++;
    while (*q && (isalnum((unsigned char)*q) || *q == '_'))
      q++;
    while (*q && isspace((unsigned char)*q))
      q++;
    if (*q != '{') {
      p = q;
      continue;
    }
    char *close = strchr(q, '}');
    if (!close)
      break;
    char *body = q + 1;
    long long next = 0;
    while (body < close && enum_count < 2048) {
      while (body < close && (*body == ',' || isspace((unsigned char)*body)))
        body++;
      if (body >= close)
        break;
      char item[256] = {0};
      size_t n = 0;
      while (body < close && *body != ',' && n < sizeof(item) - 1)
        item[n++] = *body++;
      item[n] = '\0';
      clean(item);
      char *eq = strchr(item, '=');
      char name[NAME_SZ] = {0};
      if (eq) {
        *eq = '\0';
        char *v = eq + 1;
        while (*v == ' ')
          v++;
        next = strtoll(v, NULL, 0);
      }
      snprintf(name, NAME_SZ, "%s", trim(item));
      add_enum_const(name, next);
      next++;
    }
    p = close + 1;
  }
  free(buf);
}

typedef struct {
  char name[NAME_SZ];
} BoolGlobal;

static BoolGlobal bool_globals[1024];
static int bool_global_count;

static void parse_define_constants(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line) {
    char work[512];
    snprintf(work, sizeof(work), "%s", line);
    line = strtok(NULL, "\n");
    if (enum_count >= 2048)
      break;
    char *p = work;
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
      if (!strncmp(name, "sk_", 3) || !strncmp(name, "ski_", 4) || *p == '(')
        continue;
      while (*p && isspace((unsigned char)*p))
        p++;
      char *end = p;
      while (*end && !isspace((unsigned char)*end))
        end++;
      char value[64] = {0};
      size_t vn = (size_t)(end - p);
      if (vn < sizeof(value)) {
        memcpy(value, p, vn);
        value[vn] = '\0';
      }
      if (!name[0] || !value[0] || define_count >= 2048)
        continue;
      bool duplicate = false;
      for (int i = 0; i < define_count; i++)
        if (strcmp(define_names[i], name) == 0)
          duplicate = true;
      if (duplicate)
        continue;
      snprintf(define_names[define_count], NAME_SZ, "%s", name);
      snprintf(define_exprs[define_count], 256, "%s", value);
      char num[64];
      snprintf(num, sizeof(num), "%s", value);
      size_t nn = strlen(num);
      while (nn >= 2 && num[0] == '(' && num[nn - 1] == ')') {
        num[nn - 1] = '\0';
        nn -= 2;
        memmove(num, num + 1, nn + 1);
      }
      char *tail = NULL;
      long long v = strtoll(num, &tail, 0);
      if (tail && *tail == '\0') {
        define_resolved[define_count] = true;
        add_enum_const(name, v);
      }
      define_count++;
    }
  }
  free(buf);
}

static void resolve_define_constants(void) {
  for (int pass = 0; pass < 8; pass++) {
    bool progress = false;
    for (int i = 0; i < define_count; i++) {
      if (define_resolved[i])
        continue;
      for (int j = 0; j < define_count; j++) {
        if (!define_resolved[j] ||
            strcmp(define_exprs[i], define_names[j]) != 0)
          continue;
        for (int k = 0; k < enum_count; k++) {
          if (strcmp(enum_consts[k].name, define_names[j]) == 0) {
            add_enum_const(define_names[i], enum_consts[k].value);
            define_resolved[i] = true;
            progress = true;
            break;
          }
        }
        break;
      }
    }
    if (!progress)
      break;
  }
}

static void parse_bool_globals(const char *path) {
  char *buf = read_all(path);
  if (!buf)
    return;
  char *line = strtok(buf, "\n");
  while (line && bool_global_count < 1024) {
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
  if (strcmp(sig, "void(p)") == 0)
    return "LCB_P1";
  if (strcmp(sig, "bool(p)") == 0)
    return "LCB_BP1";
  if (strncmp(sig, "bool(", 5) == 0)
    return "LCB_B";
  return "LCB_V";
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
    char arg[128] = {0};
    size_t an = 0;
    while (p[an] && p[an] != ',' && an < sizeof(arg) - 1) {
      arg[an] = p[an];
      an++;
    }
    arg[an] = '\0';
    if (strstr(arg, "char") && strchr(arg, '*')) {
      *dst++ = 's';
    } else if (strchr(arg, '*') != NULL) {
      *dst++ = 'p';
    } else if (strstr(arg, "bool")) {
      *dst++ = 'b';
    } else if (strstr(arg, "float") || strstr(arg, "double")) {
      *dst++ = 'f';
    } else if (is_scalar_type(arg)) {
      *dst++ = 'i';
    } else {
      *dst++ = 'u';
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
      if (args && cb_type_count < 1024) {
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
          char first[256] = {0};
          size_t fn = 0;
          while (args[fn] && args[fn] != ',' && fn < sizeof(first) - 1) {
            first[fn] = args[fn];
            fn++;
          }
          first[fn] = '\0';
          clean(first);
          if (strchr(first, '*'))
            snprintf(t->ptr_type, sizeof(t->ptr_type), "%s", first);
          normalize_sig(t->sig, sizeof(t->sig), ret, args);
        }
      }
    }
    line = strtok(NULL, "\n");
  }
  free(buf);
}

static FuncDesc *func_for(const char *name);
static bool can_thunk(const FuncDesc *fd);

static void lua_expr(char *out, size_t cap, const char *expr) {
  char e[256];
  snprintf(e, sizeof(e), "%s", expr);
  char *v = trim(e);
  bool is_bool_global = false;
  for (int i = 0; i < bool_global_count && !is_bool_global; i++)
    if (strcmp(v, bool_globals[i].name) == 0)
      is_bool_global = true;
  if (is_bool_global) {
    snprintf(out, cap, "native.global_b(\"%s\")", v);
    return;
  }
  for (int i = 0; i < enum_count; i++) {
    if (strcmp(v, enum_consts[i].name) == 0) {
      snprintf(out, cap, "%lld", enum_consts[i].value);
      return;
    }
  }
  if (strcmp(v, "true") == 0 || strcmp(v, "false") == 0 ||
      (v[0] >= '0' && v[0] <= '9') || v[0] == '"' || v[0] == '-') {
    snprintf(out, cap, "%s", v);
    return;
  }
  char message[512];
  snprintf(message, sizeof(message), "unresolved default expression '%s'", v);
  gen_error(message);
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

static bool is_pure_alias(const char *rest, const char *target) {
  size_t n = strlen(target);
  if (strncmp(rest, target, n) != 0)
    return false;
  const char *p = rest + n;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p == '\0')
    return true;
  if (*p != '(')
    return false;
  p++;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (strncmp(p, "__VA_ARGS__", 11) != 0)
    return false;
  p += 11;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p != ')')
    return false;
  p++;
  while (*p && isspace((unsigned char)*p))
    p++;
  return *p == '\0';
}

static void gen_wrapper(FILE *out, const char *name, const char *rest,
                        const char *params) {
  char raw[NAME_SZ] = {0};
  snprintf(raw, sizeof(raw), "ski_%s", name + 3);
  const char *kind = ret_for(raw);
  if (strcmp(kind, "v") != 0 && strcmp(kind, "i") != 0 &&
      strcmp(kind, "b") != 0 && strcmp(kind, "s") != 0 &&
      strcmp(kind, "f") != 0 && strcmp(kind, "p") != 0)
    kind = "v";

  bool raw_thunk = can_thunk(func_for(raw));
  if (can_thunk(func_for(name))) {
    fprintf(out, "M.%s = function(...) return native.%s(...) end\n", name,
            name);
    return;
  }

  if (strstr(rest, "SK_STORE_NAME")) {
    char args[8][NAME_SZ];
    int arg_count = 0;
    if (params) {
      const char *pp = strchr(params, '(');
      if (pp)
        pp++;
      while (pp && *pp && *pp != ')' && arg_count < 8) {
        while (*pp == ' ' || *pp == ',')
          pp++;
        if (!*pp || *pp == ')' || strncmp(pp, "...", 3) == 0)
          break;
        int ni = 0;
        while (*pp && ni < NAME_SZ - 1 &&
               (isalnum((unsigned char)*pp) || *pp == '_'))
          args[arg_count][ni++] = *pp++;
        args[arg_count][ni] = '\0';
        if (args[arg_count][0])
          arg_count++;
        while (*pp && *pp != ',' && *pp != ')')
          pp++;
      }
    }
    fprintf(out, "M.%s = function(", name);
    for (int i = 0; i < arg_count; i++)
      fprintf(out, "f_%s, ", args[i]);
    fprintf(out, "feature, key)\n");
    fprintf(out, "  feature = feature or native.source()\n");
    fprintf(out, "  key = key or feature\n");
    fprintf(out, "  return native.%s(", raw_thunk ? raw : kind);
    if (!raw_thunk)
      fprintf(out, "\"%s\", ", raw);
    fprintf(out, "feature, key");
    for (int i = 0; i < arg_count; i++)
      fprintf(out, ", f_%s", args[i]);
    fprintf(out, ")\nend\n");
    return;
  }
  if (strstr(rest, "__FILE__")) {
    fprintf(out, "M.%s = function(...) return native.%s(", name,
            raw_thunk ? raw : kind);
    if (!raw_thunk)
      fprintf(out, "\"%s\", ", raw);
    fprintf(out, "..., native.owner()) end\n");
    return;
  }
  if (strstr(rest, "NULL, NULL")) {
    fprintf(out, "M.%s = function(path) return native.image_size(path) end\n",
            name);
    return;
  }
  if (strstr(rest, "sk_cap_generation")) {
    fprintf(out, "M.%s = function(key) return native.cap_resolve(key) end\n",
            name);
    return;
  }
  if (strstr(rest, "(void *)") || strstr(rest, "(void*)")) {
    fprintf(out, "M.%s = function(key, fn) native.cap_register(key, fn) end\n",
            name);
    return;
  }
  if (strstr(name, "_fmt")) {
    const char *fk = ret_for(name);
    if (strcmp(fk, "v") != 0 && strcmp(fk, "i") != 0 && strcmp(fk, "b") != 0 &&
        strcmp(fk, "s") != 0 && strcmp(fk, "f") != 0 && strcmp(fk, "p") != 0)
      fk = "v";
    fprintf(out,
            "M.%s = function(...) return native.fmt_call(\"%s\", \"%s\", "
            "...) end\n",
            name, fk, name);
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
    FuncDesc *td = func_for(target);
    if (is_pure_alias(rest, target) || (td && can_thunk(td))) {
      fprintf(out, "M.%s = function(...) return M.%s(...) end\n", name, target);
      return;
    }
    fprintf(out,
            "M.%s = function(t) local v = {} for k,x in pairs(t or {}) do "
            "v[k] = x end ",
            name);
    emit_defaults(out, rest);
    fprintf(out, "return M.%s(v) end\n", target);
    return;
  }

  if (raw_thunk && strncmp(rest, "ski_", 4) == 0 &&
      is_struct_type(func_for(raw)->ret)) {
    fprintf(out,
            "M.%s = function(t) local v = {} for k,x in pairs(t or {}) do "
            "v[k] = x end ",
            name);
    emit_defaults(out, rest);
    fprintf(out, "return native.%s(v) end\n", raw);
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
    char lead_args[NAME_SZ] = {0};
    if (lead[0]) {
      snprintf(lead_args, sizeof(lead_args), "%s", lead);
      size_t ln = strlen(lead_args);
      if (ln && lead_args[ln - 1] == ',')
        lead_args[ln - 1] = '\0';
    }

    fprintf(out, "M.%s = function(t", name);
    if (lead_args[0])
      fprintf(out, ", %s", lead_args);
    fprintf(out, ") local v = {} for k,x in pairs(t or {}) do "
                 "v[k] = x end ");
    emit_defaults(out, rest);
    fprintf(out, "return native.struct_call(\"%s\", \"%s\", v, \"%s\"", target,
            stype, kind);
    if (lead_args[0])
      fprintf(out, ",%s", lead_args);
    fprintf(out, ") end\n");
    return;
  }

  if (target[0] && strcmp(target, name) != 0)
    fprintf(out, "M.%s = function(...) return M.%s(...) end\n", name, target);
  else
    fprintf(out, "function M.%s(...) return native.%s(\"%s\", ...) end\n", name,
            kind, name);
}

static char header_paths[2048][512];
static int header_count;

static void collect_headers(const char *dir) {
  DIR *d = opendir(dir);
  if (!d)
    return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    char path[512];
    snprintf(path, sizeof(path), "%.255s/%.255s", dir, e->d_name);
    struct stat st;
    if (stat(path, &st) != 0)
      continue;
    if (S_ISDIR(st.st_mode)) {
      collect_headers(path);
    } else if (S_ISREG(st.st_mode) && strstr(path, ".h") != NULL &&
               strstr(path, "/win32/") == NULL &&
               strstr(path, "/x11/") == NULL &&
               strstr(path, "/macos/") == NULL &&
               strstr(path, "/wayland/") == NULL &&
               strstr(path, "/platform/") == NULL &&
               strstr(path, "/deps/") == NULL &&
               strstr(path, "skapi_lua_generated.h") == NULL &&
               strstr(path, "lua_struct.h") == NULL && header_count < 2048) {
      snprintf(header_paths[header_count++], 512, "%s", path);
    }
  }
  closedir(d);
}

static int split_decls(const char *decl, char out[16][512]) {
  int count = 0;
  const char *start = decl;
  int depth = 0;
  char prefix[256] = {0};
  for (const char *p = decl;; p++) {
    if (*p == '(' || *p == '[')
      depth++;
    else if (*p == ')' || *p == ']')
      depth--;
    if ((*p == ',' && depth == 0) || *p == '\0') {
      size_t n = (size_t)(p - start);
      if (n > 0 && count < 16) {
        if (n > 511)
          n = 511;
        char seg[512] = {0};
        memcpy(seg, start, n);
        seg[n] = '\0';
        clean(seg);
        if (count == 0) {
          char *last_space = strrchr(seg, ' ');
          if (last_space) {
            size_t pn = (size_t)(last_space - seg + 1);
            if (pn < sizeof(prefix)) {
              memcpy(prefix, seg, pn);
              prefix[pn] = '\0';
            }
          }
          snprintf(out[count++], 512, "%s", seg);
        } else if (strchr(seg, ' ') == NULL && prefix[0]) {
          snprintf(out[count++], 512, "%.255s%.255s", prefix, seg);
        } else {
          snprintf(out[count++], 512, "%s", seg);
        }
      }
      if (*p == '\0')
        break;
      start = p + 1;
    }
  }
  return count;
}

static FuncDesc *func_for(const char *name) {
  for (int i = 0; i < func_count; i++)
    if (strcmp(funcs[i].name, name) == 0)
      return &funcs[i];
  return NULL;
}

static const char *resolve_alias_base(const char *t) {
  static char bufs[8][NAME_SZ];
  static int slot;
  slot = (slot + 1) % 8;
  snprintf(bufs[slot], NAME_SZ, "%.127s", t);
  const char *base = bufs[slot];
  for (int pass = 0; pass < 8; pass++) {
    const char *next = base;
    for (int j = 0; j < alias_count; j++)
      if (strcmp(next, aliases[j].from) == 0) {
        next = aliases[j].to;
        break;
      }
    if (strcmp(next, base) == 0)
      break;
    snprintf(bufs[slot], NAME_SZ, "%.127s", next);
    base = bufs[slot];
  }
  return base;
}

static bool is_builtin_scalar(const char *t) {
  if (strstr(t, "bool") || strstr(t, "char") || strstr(t, "short") ||
      strstr(t, "int") || strstr(t, "long") || strstr(t, "unsigned") ||
      strstr(t, "signed") || strstr(t, "size_t") || strstr(t, "float") ||
      strstr(t, "double"))
    return true;
  return false;
}

static bool is_scalar_type(const char *t) {
  const char *b = resolve_alias_base(t);
  if (is_builtin_scalar(b))
    return true;
  for (int i = 0; i < scalar_name_count; i++)
    if (strcmp(b, scalar_names[i]) == 0)
      return true;
  return false;
}

static bool is_simple_type(const char *t) {
  if (strcmp(t, "void") == 0)
    return true;
  if (strstr(t, "void *") || strstr(t, "void*") || strstr(t, "char *") ||
      strstr(t, "char*"))
    return true;
  char base[256] = {0};
  snprintf(base, sizeof(base), "%.255s", t);
  size_t n = strlen(base);
  while (n && (base[n - 1] == '*' || base[n - 1] == ' '))
    base[--n] = '\0';
  return is_scalar_type(base);
}

static bool is_struct_type(const char *t) {
  for (int i = 0; i < struct_name_count; i++) {
    if (strcmp(t, struct_names[i]) == 0)
      return true;
    const char *base = t;
    for (int pass = 0; pass < 8; pass++) {
      const char *next = base;
      for (int j = 0; j < alias_count; j++)
        if (strcmp(next, aliases[j].from) == 0) {
          next = aliases[j].to;
          break;
        }
      if (strcmp(next, base) == 0)
        break;
      base = next;
      if (strcmp(base, struct_names[i]) == 0)
        return true;
    }
  }
  return false;
}

static int callback_kind_for_type(const char *t) {
  const char *base = t;
  for (int pass = 0; pass < 8; pass++) {
    const char *next = base;
    for (int j = 0; j < alias_count; j++)
      if (strcmp(next, aliases[j].from) == 0) {
        next = aliases[j].to;
        break;
      }
    if (strcmp(next, base) == 0)
      break;
    base = next;
  }
  for (int i = 0; i < cb_type_count; i++) {
    if (strcmp(base, cb_types[i].name) != 0)
      continue;
    const char *k = cb_kind_for_sig(cb_types[i].sig);
    if (!k)
      return -1;
    if (strcmp(k, "LCB_V") == 0)
      return 0;
    if (strcmp(k, "LCB_P1") == 0)
      return 1;
    if (strcmp(k, "LCB_BP1") == 0)
      return 2;
    return 3;
  }
  if (strstr(t, "Callback") || strstr(t, "Fn") || strstr(t, "(*"))
    return 0;
  return -1;
}

static bool is_out_param(const char *t) {
  if (strchr(t, '*') == NULL)
    return false;
  if (strstr(t, "char") || strstr(t, "void") || strstr(t, "(*"))
    return false;
  char base[256];
  snprintf(base, sizeof(base), "%.255s", t);
  size_t n = strlen(base);
  while (n && (base[n - 1] == '*' || base[n - 1] == ' '))
    base[--n] = '\0';
  return is_scalar_type(base) && !is_struct_type(base);
}

static const char *struct_name_for_type(const char *t);

static const char *out_struct_type(const char *t) {
  if (!t || strchr(t, '*') == NULL)
    return NULL;
  const char *b = t;
  while (*b == ' ')
    b++;
  if (strncmp(b, "const ", 6) == 0)
    return NULL;
  if (strstr(t, "const ") == t)
    return NULL;
  return struct_name_for_type(t);
}

static bool is_out_struct_param(const char *t, const char *name) {
  if (!out_struct_type(t))
    return false;
  if (!name || !*name)
    return true;
  return strstr(name, "out") != NULL || strstr(name, "result") != NULL ||
         strstr(name, "dst") != NULL;
}

static const char *in_struct_ptr_type(const char *t, const char *name) {
  if (is_out_struct_param(t, name) || !strchr(t, '*'))
    return NULL;
  return struct_name_for_type(t);
}

static bool can_thunk(const FuncDesc *fd) {
  if (!fd || fd->varargs)
    return false;
  if (strstr(fd->params, "...") != NULL)
    return false;
  const char *k = ret_kind(fd->ret);
  if (strchr(fd->ret, '*') && strcmp(k, "p") != 0 && strcmp(k, "s") != 0)
    return false;
  if (strcmp(fd->ret, "void") != 0 && !is_simple_type(fd->ret) &&
      !is_struct_type(fd->ret) && strcmp(k, "p") != 0)
    return false;
  if (strcmp(k, "v") != 0 && strcmp(k, "b") != 0 && strcmp(k, "i") != 0 &&
      strcmp(k, "s") != 0 && strcmp(k, "f") != 0)
    return false;
  char segs[16][512];
  int n = split_decls(fd->params, segs);
  for (int i = 0; i < n; i++) {
    const char *p = segs[i];
    if (strchr(p, '[') || strstr(p, "...") || strstr(p, "**"))
      return false;
    if (strstr(p, "(*"))
      continue;
    char ptype[256] = {0};
    char pname[NAME_SZ] = {0};
    const char *pend = p + strlen(p);
    const char *pstart = pend;
    while (pstart > p &&
           (isalnum((unsigned char)pstart[-1]) || pstart[-1] == '_'))
      pstart--;
    if (pstart != pend) {
      size_t nn = (size_t)(pend - pstart);
      if (nn >= sizeof(pname))
        nn = sizeof(pname) - 1;
      memcpy(pname, pstart, nn);
      pname[nn] = '\0';
    }
    snprintf(ptype, sizeof(ptype), "%s", p);
    char *last_space = strrchr(ptype, ' ');
    if (last_space)
      *last_space = '\0';
    clean(ptype);
    bool in_struct_ptr = struct_name_for_type(ptype) != NULL &&
                         strchr(ptype, '*') != NULL &&
                         !is_out_struct_param(ptype, pname);
    if (!in_struct_ptr && strncmp(p, "const ", 6) == 0 && strchr(p, '*')) {
      char cp[256] = {0};
      snprintf(cp, sizeof(cp), "%.*s", (int)(pstart - p), p);
      clean(cp);
      if (struct_name_for_type(cp) != NULL)
        in_struct_ptr = true;
    }
    if (!is_simple_type(ptype) && !is_struct_type(ptype) &&
        callback_kind_for_type(ptype) < 0 && !is_out_param(ptype) &&
        !out_struct_type(ptype) && !in_struct_ptr)
      return false;
  }
  return true;
}

typedef struct {
  char type[256];
  char name[NAME_SZ];
} ParamDesc;

static int parse_params(const char *params, ParamDesc out[16]) {
  char segs[16][512];
  int n = split_decls(params, segs);
  int count = 0;
  for (int i = 0; i < n; i++) {
    const char *p = segs[i];
    if (strcmp(p, "void") == 0)
      continue;
    const char *end = p + strlen(p);
    const char *start = end;
    while (start > p && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
      start--;
    if (start == end)
      continue;
    ParamDesc *pd = &out[count++];
    size_t nn = (size_t)(end - start);
    if (nn >= NAME_SZ)
      nn = NAME_SZ - 1;
    memcpy(pd->name, start, nn);
    pd->name[nn] = '\0';
    size_t tn = (size_t)(start - p);
    if (tn >= sizeof(pd->type))
      tn = sizeof(pd->type) - 1;
    memcpy(pd->type, p, tn);
    pd->type[tn] = '\0';
    clean(pd->type);
  }
  return count;
}

static void base_type(const char *t, char *out, size_t cap) {
  snprintf(out, cap, "%s", t);
  size_t n = strlen(out);
  while (n && (out[n - 1] == '*' || out[n - 1] == ' '))
    out[--n] = '\0';
}

static void callback_cast_type(const char *t, char *out, size_t cap) {
  const char *p = strstr(t, "(*");
  if (!p) {
    snprintf(out, cap, "%s", t);
    return;
  }
  size_t prefix = (size_t)(p - t);
  size_t n = 0;
  for (size_t i = 0; i < prefix && n < cap - 1; i++)
    out[n++] = t[i];
  out[n++] = '(';
  out[n++] = '*';
  out[n++] = ')';
  const char *close = strchr(p, ')');
  if (close) {
    for (const char *q = close + 1; *q && n < cap - 1; q++)
      out[n++] = *q;
  }
  out[n] = '\0';
}

static bool is_handle_type(const char *t) {
  const char *b = resolve_alias_base(t);
  for (int i = 0; i < handle_name_count; i++)
    if (strcmp(b, handle_names[i]) == 0)
      return true;
  return strstr(b, "WindowHandle") != NULL;
}

static const char *value_kind_for_type(const char *t) {
  const char *b = resolve_alias_base(t);
  if (strstr(b, "bool"))
    return "b";
  if (strstr(b, "float") || strstr(b, "double"))
    return "f";
  if (strstr(b, "char") && strchr(b, '*'))
    return "s";
  return "i";
}

static void gen_thunk(FILE *out, const FuncDesc *fd) {
  const char *k = ret_kind(fd->ret);
  ParamDesc params[16];
  int n = parse_params(fd->params, params);
  fprintf(out, "static inline int lthunk_%s(lua_State *L) {\n", fd->name);
  fprintf(out, "  slua_owner_from_state(L);\n");
  int out_count = 0;
  for (int i = 0; i < n; i++) {
    const char *t = params[i].type;
    int idx = i + 1;
    int ck = callback_kind_for_type(t);
    const char *in_struct = in_struct_ptr_type(t, params[i].name);
    if (ck >= 0) {
      fprintf(out, "  void *a%d = slua_callback(L, %d, %d);\n", i, idx, ck);
    } else if (is_out_struct_param(t, params[i].name)) {
      fprintf(out, "  %s a%d; memset(&a%d, 0, sizeof(a%d));\n",
              out_struct_type(t), i, i, i);
    } else if (in_struct) {
      if (strstr(t, "const ") != NULL)
        fprintf(out,
                "  %s a%d; memset(&a%d, 0, sizeof(a%d)); "
                "slua_fill_struct(L, \"%s\", %d, &a%d);\n",
                in_struct, i, i, i, in_struct, idx, i);
      else
        fprintf(out, "  %s *a%d = (%s *)slua_raw_ptr(L, %d);\n", in_struct, i,
                in_struct, idx);
    } else if (is_struct_type(t)) {
      fprintf(out, "  %s a%d; memset(&a%d, 0, sizeof(a%d));\n", t, i, i, i);
      fprintf(out, "  slua_fill_struct(L, \"%s\", %d, &a%d);\n", t, idx, i);
    } else if (is_out_param(t)) {
      char base[256];
      base_type(t, base, sizeof(base));
      fprintf(out, "  %s a%d = 0;\n", base, i);
      out_count++;
    } else if (is_handle_type(t)) {
      fprintf(out,
              "  %s a%d; memset(&a%d, 0, sizeof(a%d)); uintptr_t v%d = "
              "lua_isnoneornil(L, %d) ? 0 : (uintptr_t)lua_tointeger(L, %d); "
              "memcpy(&a%d, &v%d, sizeof(a%d) < sizeof(uintptr_t) ? "
              "sizeof(a%d) : sizeof(uintptr_t));\n",
              t, i, i, i, i, idx, idx, i, i, i, i);
    } else if (strstr(t, "char **") || strstr(t, "char**") ||
               strstr(t, "char *const *")) {
      fprintf(out, "  char **a%d = (char **)slua_string_array(L, %d);\n", i,
              idx);
    } else if (strcmp(value_kind_for_type(t), "s") == 0) {
      fprintf(out,
              "  const char *a%d = lua_isnoneornil(L, %d) ? \"\" : "
              "luaL_checkstring(L, %d);\n",
              i, idx, idx);
    } else if (strcmp(value_kind_for_type(t), "b") == 0 && !strchr(t, '*')) {
      fprintf(out, "  bool a%d = lua_toboolean(L, %d) != 0;\n", i, idx);
    } else if (strcmp(value_kind_for_type(t), "f") == 0 && !strchr(t, '*')) {
      fprintf(out,
              "  double a%d = lua_isnoneornil(L, %d) ? 0 : "
              "lua_tonumber(L, %d);\n",
              i, idx, idx);
    } else if (strchr(t, '*')) {
      fprintf(out,
              "  void *a%d = (void *)(uintptr_t)(lua_isnoneornil(L, %d) ? 0 : "
              "lua_tointeger(L, %d));\n",
              i, idx, idx);
    } else {
      fprintf(out,
              "  long a%d = lua_isnoneornil(L, %d) ? 0 : "
              "lua_tointeger(L, %d);\n",
              i, idx, idx);
    }
  }
  fprintf(out, "  void *sym = Platform_GetMainSymbol(\"%s\");\n", fd->name);
  fprintf(out,
          "  if (!sym) { slua_owner_clear(); return luaL_error(L, "
          "\"unknown skapi function '%%s'\", \"%s\"); }\n",
          fd->name);
  fprintf(out, "  typedef %s (*fn_t)(", fd->ret);
  for (int i = 0; i < n; i++) {
    if (i)
      fprintf(out, ", ");
    fprintf(out, "%s", params[i].type);
  }
  if (n == 0)
    fprintf(out, "void");
  fprintf(out, ");\n  ");
  if (strcmp(k, "v") != 0)
    fprintf(out, "%s r = ", fd->ret);
  fprintf(out, "((fn_t)sym)(");
  for (int i = 0; i < n; i++) {
    if (i)
      fprintf(out, ", ");
    const char *t = params[i].type;
    if (is_out_struct_param(t, params[i].name))
      fprintf(out, "&a%d", i);
    else if (is_struct_type(t) || is_handle_type(t))
      fprintf(out, "a%d", i);
    else if (in_struct_ptr_type(t, params[i].name)) {
      if (strstr(t, "const ") != NULL)
        fprintf(out, "&a%d", i);
      else
        fprintf(out, "a%d", i);
    } else if (callback_kind_for_type(t) >= 0) {
      char ct[512];
      callback_cast_type(t, ct, sizeof(ct));
      fprintf(out, "(%s)a%d", ct, i);
    } else if (is_out_param(t))
      fprintf(out, "&a%d", i);
    else
      fprintf(out, "(%s)a%d", t, i);
  }
  fprintf(out, ");\n  slua_owner_clear();\n");
  if (is_struct_type(fd->ret)) {
    fprintf(out, "  slua_push_struct(L, \"%s\", &r);\n", fd->ret);
  } else if (is_handle_type(fd->ret)) {
    fprintf(out, "  uintptr_t rv = 0; memcpy(&rv, &r, sizeof(rv) < sizeof(r) "
                 "? sizeof(rv) : sizeof(r)); lua_pushinteger(L, "
                 "(lua_Integer)rv);\n");
  } else if (strcmp(k, "p") == 0)
    fprintf(out, "  lua_pushlightuserdata(L, (void *)(uintptr_t)r);\n");
  else if (strcmp(k, "b") == 0)
    fprintf(out, "  lua_pushboolean(L, r != 0);\n");
  else if (strcmp(k, "i") == 0)
    fprintf(out, "  lua_pushinteger(L, (lua_Integer)r);\n");
  else if (strcmp(k, "s") == 0)
    fprintf(out, "  lua_pushstring(L, r ? r : \"\");\n");
  else if (strcmp(k, "f") == 0)
    fprintf(out, "  lua_pushnumber(L, (double)r);\n");
  for (int i = 0; i < n; i++) {
    const char *osn = is_out_struct_param(params[i].type, params[i].name)
                          ? out_struct_type(params[i].type)
                          : NULL;
    if (osn) {
      fprintf(out, "  slua_push_struct(L, \"%s\", &a%d);\n", osn, i);
      continue;
    }
    if (!is_out_param(params[i].type))
      continue;
    char base[256];
    base_type(params[i].type, base, sizeof(base));
    const char *ok = value_kind_for_type(base);
    if (strcmp(ok, "b") == 0)
      fprintf(out, "  lua_pushboolean(L, a%d != 0);\n", i);
    else if (strcmp(ok, "f") == 0)
      fprintf(out, "  lua_pushnumber(L, (double)a%d);\n", i);
    else if (strcmp(ok, "s") == 0)
      fprintf(out, "  lua_pushstring(L, a%d ? a%d : \"\");\n", i, i);
    else
      fprintf(out, "  lua_pushinteger(L, (lua_Integer)a%d);\n", i);
  }
  int extra = out_count;
  for (int i = 0; i < n; i++)
    if (is_out_struct_param(params[i].type, params[i].name))
      extra++;
  fprintf(out, "  return %d;\n}\n", (strcmp(k, "v") == 0 ? 0 : 1) + extra);
}

static void gen_thunk_registry(FILE *out) {
  fprintf(out, "static inline void slua_register_thunks(void *Lp) {\n");
  fprintf(out, "  lua_State *L = (lua_State *)Lp;\n");
  for (int i = 0; i < func_count; i++)
    if (can_thunk(&funcs[i]))
      fprintf(
          out,
          "  lua_pushcfunction(L, lthunk_%s); lua_setfield(L, -2, \"%s\");\n",
          funcs[i].name, funcs[i].name);
  fprintf(out, "}\n");
}

static char *find_field_decl(const char *body, const char *end,
                             const char *name) {
  size_t n = strlen(name);
  for (const char *p = body; p + n <= end; p++) {
    if (strncmp(p, name, n) != 0)
      continue;
    bool left_ok =
        p == body || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
    bool right_ok =
        p[n] == '\0' || !(isalnum((unsigned char)p[n]) || p[n] == '_');
    if (left_ok && right_ok)
      return (char *)p;
  }
  return NULL;
}

static int count_field_bytes(const char *found, const char *body_start) {
  char rev[128] = {0};
  size_t n = 0;
  const char *p = found;
  while (p > body_start && n + 1 < sizeof(rev)) {
    --p;
    if (*p == ';' || *p == '{' || *p == '}') {
      p++;
      break;
    }
    rev[n++] = *p;
  }
  char text[128];
  for (size_t i = 0; i < n; i++)
    text[i] = rev[n - 1 - i];
  text[n] = '\0';
  if (strstr(text, "size_t") || strstr(text, "uint64") ||
      strstr(text, "unsigned long") || strstr(text, "long long"))
    return 8;
  if (strstr(text, "short"))
    return 2;
  return 4;
}

static const char *struct_name_for_type(const char *t) {
  char base[256] = {0};
  snprintf(base, sizeof(base), "%.255s", t);
  size_t n = strlen(base);
  while (n && (base[n - 1] == '*' || base[n - 1] == ' '))
    base[--n] = '\0';
  char *star = strchr(base, '*');
  if (star) {
    const char *q = star + 1;
    while (*q == ' ')
      q++;
    if (*q)
      *star = '\0';
  }
  n = strlen(base);
  while (n && (base[n - 1] == '*' || base[n - 1] == ' '))
    base[--n] = '\0';
  if (strncmp(base, "const ", 6) == 0)
    memmove(base, base + 6, strlen(base) - 5);
  const char *resolved = resolve_alias_base(base);
  for (int i = 0; i < struct_name_count; i++)
    if (strcmp(resolved, struct_names[i]) == 0)
      return struct_names[i];
  return NULL;
}

static void collect_rets_from_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  static char rows[MAX_LINES][LINE_SZ];
  int count = 0;
  char buf[LINE_SZ];
  while (fgets(buf, sizeof(buf), f) && count < MAX_LINES) {
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
      buf[--n] = '\0';
    snprintf(rows[count++], LINE_SZ, "%s", buf);
  }
  fclose(f);
  for (int i = 0; i < count; i++) {
    char work[LINE_SZ];
    snprintf(work, sizeof(work), "%s", rows[i]);
    char *p = work;
    while (*p && isspace((unsigned char)*p))
      p++;
    char *semi = strrchr(p, ';');
    if (!semi || !strchr(p, '('))
      continue;
    *semi = '\0';
    char lines[1][LINE_SZ];
    snprintf(lines[0], LINE_SZ, "%s", p);
    collect_rets(lines, 1);
  }
}

static int gen_struct_header(void) {
  collect_headers("include");
  for (int i = 0; i < header_count; i++)
    collect_rets_from_file(header_paths[i]);

  for (int i = 0; i < header_count; i++) {
    parse_callback_typedefs(header_paths[i]);
    parse_enum_typedefs(header_paths[i]);
    parse_enum_values(header_paths[i]);
    parse_define_constants(header_paths[i]);
    parse_bool_globals(header_paths[i]);
  }
  resolve_define_constants();
  FILE *out = fopen(out_path("include/core/skapi_lua_generated.h.tmp"), "w");
  if (!out) {
    fprintf(stderr,
            "gen_skapi_lua: cannot write generated struct header.tmp\n");
    return 1;
  }
  fprintf(out, "#ifndef SKAPI_LUA_GENERATED_H\n"
               "#define SKAPI_LUA_GENERATED_H\n\n"
               "#include \"core/skapi.h\"\n"
               "#include \"core/lua_host.h\"\n"
               "#include \"core/lua_struct.h\"\n"
               "#include \"platform/lib.h\"\n"
               "#include \"deps/lua/lua.h\"\n"
               "#include \"deps/lua/lauxlib.h\"\n"
               "#include <stdint.h>\n"
               "#include <string.h>\n\n");

  for (int h = 0; h < header_count; h++)
    parse_aliases(header_paths[h]);
  for (int i = 0; i < ret_count; i++)
    rets[i].kind = ret_kind(rets[i].type);

  for (int h = 0; h < header_count; h++) {
    char *buf = read_all(header_paths[h]);
    if (!buf) {
      fprintf(stderr, "cannot read %s\n", header_paths[h]);
      continue;
    }
    char *p = buf;
    while ((p = strstr(p, "struct")) != NULL) {
      if (p > buf && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
        p += 6;
        continue;
      }
      char *q = p + 6;
      while (*q && isspace((unsigned char)*q))
        q++;
      char tag[NAME_SZ] = {0};
      int tag_i = 0;
      while (*q && (isalnum((unsigned char)*q) || *q == '_')) {
        if (tag_i < NAME_SZ - 1)
          tag[tag_i++] = *q;
        q++;
      }
      tag[tag_i] = '\0';
      while (*q && isspace((unsigned char)*q))
        q++;
      if (*q != '{') {
        p = q;
        continue;
      }
      char *open = q;
      char *close = strchr(open, '}');
      if (!close)
        break;
      char *after = strchr(close, ';');
      if (!after)
        break;
      bool conditional = false;
      for (char *d = open + 1; d < close; d++) {
        if (*d == '#')
          conditional = true;
      }
      if (conditional) {
        if (tag[0] && strstr(tag, "Handle") && handle_name_count < 256)
          snprintf(handle_names[handle_name_count++], NAME_SZ, "%s", tag);
        p = after + 1;
        goto next_struct;
      }
      char name[NAME_SZ] = {0};
      if (tag[0]) {
        snprintf(name, NAME_SZ, "%s", tag);
      } else {
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
      }
      int existing = 0;
      for (int si = 0; si < struct_name_count && !existing; si++)
        existing = strcmp(struct_names[si], name) == 0;
      if (!name[0] || existing || struct_name_count >= 2048) {
        p = after + 1;
        continue;
      }
      snprintf(struct_names[struct_name_count], NAME_SZ, "%s", name);
      snprintf(struct_sources[struct_name_count], 512, "%.511s",
               header_paths[h]);
      struct_name_count++;
      {
        const char *rel = struct_sources[struct_name_count - 1];
        if (strncmp(rel, "include/", 8) == 0)
          rel += 8;
        fprintf(out, "#include \"%s\"\n", rel);
        fprintf(out, "static const LuaFieldDesc lf_%s[] = {\n", name);
        char *body = open + 1;
        char *body_start = body;
        char *end = close;
        while (body < end) {
          char *semi = strchr(body, ';');
          if (!semi || semi > end)
            break;
          char raw_decl[512] = {0};
          size_t n = (size_t)(semi - body);
          if (n > sizeof(raw_decl) - 1)
            n = sizeof(raw_decl) - 1;
          memcpy(raw_decl, body, n);
          raw_decl[n] = '\0';
          clean(raw_decl);
          body = semi + 1;

          char decls[16][512];
          int decl_count = split_decls(raw_decl, decls);
          for (int di = 0; di < decl_count; di++) {
            char decl[512] = {0};
            snprintf(decl, sizeof(decl), "%s", decls[di]);
            char field[NAME_SZ] = {0};
            LuaFieldKind kind = LSF_SKIP;
            int cb_kind = 0;
            char aux[2048] = "0";
            char target_name[NAME_SZ] = {0};
            char *fptr = strstr(decl, "(*");
            if (fptr) {
              int fi = 0;
              char *q2 = fptr + 2;
              while (*q2 && fi < NAME_SZ - 1 && *q2 != ')')
                field[fi++] = *q2++;
              field[fi] = '\0';
              char *args = strstr(q2, ")(");
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
                    else if (strcmp(ck, "LCB_P1") == 0)
                      cb_kind = 1;
                    else if (strcmp(ck, "LCB_BP1") == 0)
                      cb_kind = 2;
                    else
                      cb_kind = 3;
                  }
                }
              }
            } else {
              char *last = NULL;
              for (char *q2 = decl; *q2; q2++)
                if (isspace((unsigned char)*q2) || *q2 == '*')
                  last = q2;
              if (last) {
                int fi = 0;
                char *q2 = last + 1;
                while (*q2 && fi < NAME_SZ - 1 &&
                       (isalnum((unsigned char)*q2) || *q2 == '_'))
                  field[fi++] = *q2++;
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
                            : strcmp(ck, "LCB_P1") == 0  ? 1
                            : strcmp(ck, "LCB_BP1") == 0 ? 2
                                                         : 3;
                }
                is_cb_type = true;
              }
              if (strchr(decl, '[')) {
                char *br0 = strchr(decl, '[');
                bool ptr_array = false;
                for (char *q2 = decl; q2 < br0; q2++)
                  if (*q2 == '*')
                    ptr_array = true;
                if (!strstr(decl, "char"))
                  goto skip_field;
                if (!ptr_array)
                  kind = LSF_STR_INLINE;
                else
                  kind = LSF_STR_ARRAY;
                char *br = strchr(decl, '[') + 1;
                char sz[128] = {0};
                size_t zi = 0;
                while (*br && *br != ']' && zi < sizeof(sz) - 1)
                  sz[zi++] = *br++;
                sz[zi] = '\0';
                clean(sz);
                bool digits = sz[0] != '\0';
                for (char *dd = sz; *dd; dd++)
                  if (!isdigit((unsigned char)*dd))
                    digits = false;
                if (digits)
                  snprintf(aux, sizeof(aux), "%s", sz);
                else
                  snprintf(
                      aux, sizeof(aux),
                      "sizeof(((%.*s *)0)->%.*s)/sizeof(((%.*s *)0)->%.*s[0])",
                      120, name, 120, field, 120, name, 120, field);
              } else if (is_cb_type) {
                ;
              } else {
                bool is_str_list = strstr(decl, "char **") != NULL ||
                                   strstr(decl, "char**") != NULL;
                if (is_str_list) {
                  const char *cands[4] = {NULL, NULL, NULL, NULL};
                  char c0[NAME_SZ + 8], c1[NAME_SZ + 8];
                  snprintf(c0, sizeof(c0), "%.120s_count", field);
                  snprintf(c1, sizeof(c1), "%.120s_size", field);
                  cands[0] = c0;
                  cands[1] = c1;
                  if (strcmp(field, "items") == 0) {
                    cands[2] = "item_count";
                    cands[3] = "count";
                  }
                  for (int ci = 0; ci < 4 && kind == LSF_SKIP; ci++) {
                    if (!cands[ci])
                      continue;
                    char *found = find_field_decl(body_start, close, cands[ci]);
                    if (found && found < close) {
                      kind = LSF_STR_LIST;
                      snprintf(aux, sizeof(aux), "offsetof(%.60s, %.60s)", name,
                               cands[ci]);
                      cb_kind = count_field_bytes(found, body_start);
                    }
                  }
                }
                if (kind == LSF_SKIP) {
                  char size_name[NAME_SZ + 8];
                  snprintf(size_name, sizeof(size_name), "%.120s_size", field);
                  char *sz = strstr(body_start, size_name);
                  if (sz && sz < close &&
                      (strstr(decl, "char *") || strstr(decl, "char*"))) {
                    kind = LSF_BUFFER;
                    snprintf(aux, sizeof(aux), "offsetof(%.60s, %.60s)", name,
                             size_name);
                    cb_kind = count_field_bytes(sz, body_start);
                  }
                }
              }
              if (kind == LSF_SKIP) {
                if (strchr(decl, '*')) {
                  char ftype[256] = {0};
                  char *fp = strstr(decl, field);
                  size_t tn = fp ? (size_t)(fp - decl) : strlen(decl);
                  if (tn >= sizeof(ftype))
                    tn = sizeof(ftype) - 1;
                  memcpy(ftype, decl, tn);
                  ftype[tn] = '\0';
                  clean(ftype);
                  const char *sn = struct_name_for_type(ftype);
                  if (sn) {
                    kind = LSF_STRUCT_PTR;
                    snprintf(target_name, NAME_SZ, "%s", sn);
                  }
                }
                if (kind == LSF_SKIP &&
                    (strstr(decl, "char *") || strstr(decl, "char*") ||
                     strstr(decl, "const char")))
                  kind = LSF_STR;
                else if (kind == LSF_SKIP && strchr(decl, '*'))
                  kind = LSF_PTR;
                else if (strstr(decl, "bool") == decl)
                  kind = LSF_BOOL;
                else if (strstr(decl, "float") == decl)
                  kind = LSF_FLOAT;
                else if (strstr(decl, "double") == decl)
                  kind = LSF_DOUBLE;
                else {
                  char ftype[256] = {0};
                  snprintf(ftype, sizeof(ftype), "%.255s", decl);
                  char *sp = strrchr(ftype, ' ');
                  if (sp)
                    *sp = '\0';
                  if (is_scalar_type(ftype))
                    kind = LSF_INT;
                }
              }
            }
          skip_field:
            char target_quoted[NAME_SZ + 4];
            if (target_name[0])
              snprintf(target_quoted, sizeof(target_quoted), "\"%s\"",
                       target_name);
            else
              snprintf(target_quoted, sizeof(target_quoted), "NULL");
            if (field[0] && kind != LSF_SKIP)
              fprintf(out, "    {\"%s\", %s, offsetof(%s, %s), %s, %d, %s},\n",
                      field,
                      kind == LSF_INT          ? "LSF_INT"
                      : kind == LSF_BOOL       ? "LSF_BOOL"
                      : kind == LSF_FLOAT      ? "LSF_FLOAT"
                      : kind == LSF_DOUBLE     ? "LSF_DOUBLE"
                      : kind == LSF_STR        ? "LSF_STR"
                      : kind == LSF_STR_ARRAY  ? "LSF_STR_ARRAY"
                      : kind == LSF_STR_LIST   ? "LSF_STR_LIST"
                      : kind == LSF_STR_INLINE ? "LSF_STR_INLINE"
                      : kind == LSF_BUFFER     ? "LSF_BUFFER"
                      : kind == LSF_PTR        ? "LSF_PTR"
                      : kind == LSF_STRUCT_PTR ? "LSF_STRUCT_PTR"
                                               : "LSF_CB",
                      name, field, aux, cb_kind, target_quoted);
          }
        }
        fprintf(out, "};\n");
        fprintf(out,
                "static const LuaStructDesc lsd_%s = {\"%s\", sizeof(%s), "
                "lf_%s, (int)(sizeof(lf_%s)/sizeof(lf_%s[0]))};\n",
                name, name, name, name, name, name);
      }
      p = after + 1;
    next_struct:;
    }
    free(buf);
  }

  fprintf(out, "static const LuaStructDesc *lua_generated_structs[] = {\n");
  for (int i = 0; i < struct_name_count; i++)
    fprintf(out, "    &lsd_%s,\n", struct_names[i]);
  fprintf(out, "};\n");
  fprintf(out, "static const char *lua_alias_from[] = {\n");
  for (int i = 0; i < alias_count; i++)
    fprintf(out, "    \"%s\",\n", aliases[i].from);
  fprintf(out, "};\nstatic const char *lua_alias_to[] = {\n");
  for (int i = 0; i < alias_count; i++)
    fprintf(out, "    \"%s\",\n", aliases[i].to);
  fprintf(out,
          "};\n"
          "static const LuaStructDesc *slua_struct_find(const char *name) {\n"
          "  for (int pass = 0; pass < 8; pass++) {\n"
          "    const char *next = name;\n"
          "    for (size_t i = 0; i < "
          "sizeof(lua_alias_from)/sizeof(lua_alias_from[0]); i++)\n"
          "      if (strcmp(next, lua_alias_from[i]) == 0) {\n"
          "        next = lua_alias_to[i];\n"
          "        break;\n"
          "      }\n"
          "    if (strcmp(next, name) == 0)\n"
          "      break;\n"
          "    name = next;\n"
          "  }\n"
          "  for (size_t i = 0; i < "
          "sizeof(lua_generated_structs)/sizeof(lua_generated_structs[0]); "
          "i++)\n"
          "    if (strcmp(name, lua_generated_structs[i]->name) == 0)\n"
          "      return lua_generated_structs[i];\n"
          "  return NULL;\n"
          "}\n\n");
  char cb_struct[4][NAME_SZ] = {{0}};
  for (int pass = 2; pass >= 0; pass--) {
    for (int i = 0; i < cb_type_count; i++) {
      const char *k = cb_kind_for_sig(cb_types[i].sig);
      int ki = -1;
      if (k && strcmp(k, "LCB_P1") == 0)
        ki = 1;
      else if (k && strcmp(k, "LCB_BP1") == 0)
        ki = 2;
      if (ki < 0 || !cb_types[i].ptr_type[0])
        continue;
      if (pass < 2 && cb_struct[ki][0])
        continue;
      const char *sn = struct_name_for_type(cb_types[i].ptr_type);
      if (!sn)
        continue;
      int score = strstr(sn, "Prompt") ? 2 : (strstr(sn, "State") ? 1 : 0);
      if (score != pass)
        continue;
      snprintf(cb_struct[ki], NAME_SZ, "%s", sn);
    }
  }
  char cb_val[4][NAME_SZ + 4];
  for (int i = 0; i < 4; i++) {
    if (cb_struct[i][0])
      snprintf(cb_val[i], sizeof(cb_val[i]), "\"%s\"", cb_struct[i]);
    else
      snprintf(cb_val[i], sizeof(cb_val[i]), "NULL");
  }
  fprintf(out,
          "const char *slua_cb_struct_name(int kind) {\n"
          "  switch (kind) {\n"
          "  case 1: return %s;\n"
          "  case 2: return %s;\n"
          "  default: return NULL;\n"
          "  }\n"
          "}\n\n",
          cb_val[1], cb_val[2]);

  for (int i = 0; i < func_count; i++)
    if (can_thunk(&funcs[i]))
      gen_thunk(out, &funcs[i]);
  gen_thunk_registry(out);
  fprintf(out, "\n#endif\n");
  fclose(out);
  write_if_changed(out_path("include/core/skapi_lua_generated.h"),
                   out_path("include/core/skapi_lua_generated.h.tmp"));
  return 0;
}

static void validate_ret_kinds(void) {
  for (int i = 0; i < func_count; i++) {
    const char *t = funcs[i].ret;
    if (!t[0] || strcmp(t, "void") == 0 || is_simple_type(t) ||
        is_struct_type(t) || is_handle_type(t) || strchr(t, '*') ||
        strstr(t, "Handle") || strcmp(t, "SkLibrary") == 0 ||
        strcmp(t, "SkThread") == 0 || strcmp(t, "SkMutex") == 0)
      continue;
    char message[256];
    snprintf(message, sizeof(message), "unsupported return type '%s' for '%s'",
             t, funcs[i].name);
    gen_error(message);
  }
}

int main(int argc, char **argv) {
  if (argc > 1 && argv[1])
    snprintf(g_out_dir, sizeof(g_out_dir), "%s", argv[1]);
  FILE *in = fopen("include/core/skapi.h", "r");
  if (!in) {
    fprintf(stderr, "gen_skapi_lua: cannot open include/core/skapi.h\n");
    return 1;
  }
  FILE *out = fopen(out_path("src/skapi.lua.tmp"), "w");
  if (!out) {
    fclose(in);
    fprintf(stderr, "gen_skapi_lua: cannot write src/skapi.lua.tmp\n");
    return 1;
  }

  static char lines[MAX_LINES][LINE_SZ];
  int line_count = 0;
  int cur = -1;
  bool in_macro = false;
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
      in_macro = strncmp(buf, "#define ", 8) == 0;
      snprintf(lines[cur], LINE_SZ, "%s", buf);
    } else {
      size_t used = strlen(lines[cur]);
      snprintf(lines[cur] + used, LINE_SZ - used, "%s", buf);
    }
    if (!cont) {
      char *s = buf;
      while (*s && isspace((unsigned char)*s))
        s++;
      bool preproc = *s == '#';
      bool join = *s != '\0' && !preproc && strchr(buf, ';') == NULL &&
                  (strchr(buf, '(') != NULL || strncmp(s, "typedef", 7) == 0);
      cur = in_macro ? -1 : (join ? cur : -1);
      if (cur < 0)
        in_macro = false;
    }
  }
  fclose(in);
  collect_rets(lines, line_count);
  int gen_rc = gen_struct_header();
  validate_ret_kinds();

  fprintf(
      out,
      "-- Generated by tools/gen_skapi_lua.c. Do not edit.\n"
      "local native = require(\"skapi_native\")\n"
      "local M = {}\n\n"
      "M.struct_from_ptr = function(name, ptr) return "
      "native.struct_from_ptr(name, ptr) end\n"
      "M.global_i = function(name) return native.global_i(name) end\n"
      "M.font_size = function() return native.global_i(\"FontSize\") end\n");

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
      if (!name[0] || name[0] != 's' || name[1] != 'k' ||
          strncmp(name, "ski_", 4) == 0)
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
    if (!fname[0] || fname[0] != 's' || fname[1] != 'k' ||
        strncmp(fname, "ski_", 4) == 0)
      continue;
    if (strstr(p, "typedef"))
      continue;
    if (strcmp(fname, "sk_cap_resolve") == 0) {
      fprintf(out,
              "M.cap_resolve = function(key) return native.cap_resolve(key) "
              "end\n");
      continue;
    }
    if (strcmp(fname, "sk_cap_resolve_abi") == 0) {
      fprintf(out, "M.cap_resolve_abi = function(key, abi) return "
                   "native.cap_resolve(key, abi) end\n");
      continue;
    }
    if (strcmp(fname, "sk_cap_register_abi") == 0) {
      fprintf(out, "M.cap_register_abi = function(key, abi, fn) "
                   "native.cap_register(key, fn, abi) end\n");
      continue;
    }
    if (strcmp(fname, "sk_cap_bind_declare") == 0) {
      fprintf(out, "M.cap_bind_declare = function(key) "
                   "native.cap_bind_declare(key) end\n");
      continue;
    }
    if (strstr(fname, "_fmt")) {
      gen_wrapper(out, fname, p, "");
    } else if (can_thunk(func_for(fname))) {
      fprintf(out, "M.%s = function(...) return native.%s(...) end\n", fname,
              fname);
    } else {
      fprintf(out, "function M.%s(...) return native.%s(\"%s\", ...) end\n",
              fname, ret_kind(p), fname);
    }
  }

  for (int i = 0; i < enum_count; i++)
    fprintf(out, "M.%s = %lld\n", enum_consts[i].name, enum_consts[i].value);

  fprintf(out, "\nreturn M\n");
  fclose(out);

  FILE *in2 = fopen(out_path("src/skapi.lua.tmp"), "rb");
  FILE *dst = fopen(out_path("src/skapi.lua.new"), "wb");
  if (in2 && dst) {
    char line[LINE_SZ];
    while (fgets(line, sizeof(line), in2)) {
      char *p = line;
      while ((p = strstr(p, "M.ski_")) != NULL) {
        memmove(p + 2, p + 5, strlen(p + 5) + 1);
        memcpy(p, "M.i_", 4);
        p += 4;
      }
      p = line;
      while ((p = strstr(p, "M.sk_")) != NULL) {
        memmove(p + 2, p + 5, strlen(p + 5) + 1);
        p += 2;
      }
      fputs(line, dst);
    }
  }
  if (in2)
    fclose(in2);
  if (dst)
    fclose(dst);
  remove(out_path("src/skapi.lua.tmp"));
  write_if_changed(out_path("src/skapi.lua"), out_path("src/skapi.lua.new"));
  return gen_rc != 0 || gen_errors != 0 ? 1 : 0;
}
