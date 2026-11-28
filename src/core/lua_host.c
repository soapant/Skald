#include "core/lua_host.h"
#include "config_values.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/lua/lualib.h"

#include "core/skapi_lua_generated.h"
#include "core/cap_registry.h"
#include "core/owner_purge.h"
#include "core/window_manager.h"
#include "deps/log.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_owner.h"
#include "platform/lib.h"

typedef struct {
  char *name;
  char *owner;
  lua_State *L;
  int *refs;
  int ref_count;
  int ref_cap;
  void **allocs;
  int alloc_count;
  int alloc_cap;
} LuaFeature;

static LuaFeature *g_features;
static int g_feature_count;
static int g_feature_cap;

static __thread char *tl_owner;

const char *slua_current_owner(void) { return tl_owner; }

void slua_owner_set(const char *owner) {
  free(tl_owner);
  tl_owner = owner ? strdup(owner) : NULL;
}

void slua_owner_clear(void) {
  free(tl_owner);
  tl_owner = NULL;
}

static LuaFeature *lua_feature(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "skapi_feature");
  LuaFeature *f = (LuaFeature *)lua_touserdata(L, -1);
  lua_pop(L, 1);
  return f;
}

static void track_ref(LuaFeature *f, int ref) {
  if (!f)
    return;
  if (f->ref_count >= f->ref_cap) {
    int cap = f->ref_cap > 0 ? f->ref_cap * 2 : 16;
    int *grown = (int *)realloc(f->refs, (size_t)cap * sizeof(int));
    if (!grown)
      return;
    f->refs = grown;
    f->ref_cap = cap;
  }
  f->refs[f->ref_count++] = ref;
}

static void track_alloc(LuaFeature *f, void *p) {
  if (!f || !p)
    return;
  if (f->alloc_count >= f->alloc_cap) {
    int cap = f->alloc_cap > 0 ? f->alloc_cap * 2 : 8;
    void **grown = (void **)realloc(f->allocs, (size_t)cap * sizeof(void *));
    if (!grown)
      return;
    f->allocs = grown;
    f->alloc_cap = cap;
  }
  f->allocs[f->alloc_count++] = p;
}

static void untrack_ref(LuaFeature *f, int ref) {
  if (!f)
    return;
  for (int i = 0; i < f->ref_count; i++) {
    if (f->refs[i] != ref)
      continue;
    f->refs[i] = f->refs[f->ref_count - 1];
    f->ref_count--;
    return;
  }
}

static void untrack_refs(LuaFeature *f) {
  if (!f || !f->L)
    return;
  for (int i = 0; i < f->ref_count; i++)
    luaL_unref(f->L, LUA_REGISTRYINDEX, f->refs[i]);
  free(f->refs);
  f->refs = NULL;
  f->ref_count = 0;
  f->ref_cap = 0;
}

enum { CB_V, CB_P1, CB_BP1, CB_B };

#define CB_V_COUNT 128
#define CB_P1_COUNT 32
#define CB_BP1_COUNT 32
#define CB_B_COUNT 32
#define CB_TOTAL (CB_V_COUNT + CB_P1_COUNT + CB_BP1_COUNT + CB_B_COUNT)

#define CB_V_SLOTS                                                             \
  X(0)                                                                         \
  X(1)                                                                         \
  X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15)  \
      X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27)  \
          X(28) X(29) X(30) X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38)    \
              X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48)      \
                  X(49) X(50) X(51) X(52) X(53) X(54) X(55) X(56) X(57) X(58)  \
                      X(59) X(60) X(61) X(62) X(63) X(64) X(65) X(66) X(67)    \
                          X(68) X(69) X(70) X(71) X(72) X(73) X(74) X(75)      \
                              X(76) X(77) X(78) X(79) X(80) X(81) X(82) X(83)  \
                                  X(84) X(85) X(86) X(87) X(88) X(89) X(90)    \
                                      X(91) X(92) X(93) X(94) X(95) X(96) X(   \
                                          97) X(98) X(99) X(100) X(101) X(102) \
                                          X(103) X(104) X(105) X(106) X(107)   \
                                              X(108) X(109) X(110) X(111) X(   \
                                                  112) X(113) X(114) X(115)    \
                                                  X(116) X(117) X(118) X(119)  \
                                                      X(120) X(121) X(122)     \
                                                          X(123) X(124) X(125) \
                                                              X(126) X(127)
#define CB_P1_SLOTS                                                            \
  X(0)                                                                         \
  X(1)                                                                         \
  X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15)  \
      X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27)  \
          X(28) X(29) X(30) X(31)
#define CB_BP1_SLOTS CB_P1_SLOTS
#define CB_B_SLOTS CB_P1_SLOTS

static int cb_refs[CB_TOTAL];
static LuaFeature *cb_owners[CB_TOTAL];
static bool cb_in_use[CB_TOTAL];

static void cb_invoke(int slot, int nargs, bool want_bool, bool *out_bool,
                      void *arg, int kind) {
  LuaFeature *f = cb_owners[slot];
  if (!f || !f->L)
    return;
  cb_in_use[slot] = true;
  const char *saved = slua_current_owner();
  char *saved_copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f->owner);
  lua_rawgeti(f->L, LUA_REGISTRYINDEX, cb_refs[slot]);
  if (nargs == 1) {
    const char *sname = arg ? slua_cb_struct_name(kind) : NULL;
    if (sname)
      slua_push_struct(f->L, sname, arg);
    else
      lua_pushlightuserdata(f->L, arg);
  }
  if (lua_pcall(f->L, nargs, want_bool ? 1 : 0, 0) != 0) {
    const char *msg = lua_tostring(f->L, -1);
    if (!msg)
      msg = luaL_typename(f->L, -1);
    log_error("lua callback error: %s", msg);
    luaL_traceback(f->L, f->L, msg, 1);
    const char *tb = lua_tostring(f->L, -1);
    if (tb)
      log_error("lua callback traceback:\n%s", tb);
    lua_pop(f->L, 2);
    if (want_bool && out_bool)
      *out_bool = false;
  } else if (want_bool && out_bool) {
    *out_bool = lua_toboolean(f->L, -1) != 0;
    lua_pop(f->L, 1);
  }
  cb_in_use[slot] = false;
  slua_owner_clear();
  if (saved_copy) {
    slua_owner_set(saved_copy);
    free(saved_copy);
  }
}

#define DEF_CB_V(N)                                                            \
  static void lua_cb_v_##N(void) { cb_invoke(N, 0, false, NULL, NULL, CB_V); }
#define DEF_CB_P1(N)                                                           \
  static void lua_cb_p1_##N(void *arg) {                                       \
    cb_invoke(CB_V_COUNT + N, 1, false, NULL, arg, CB_P1);                     \
  }
#define DEF_CB_BP1(N)                                                          \
  static bool lua_cb_bp1_##N(void *arg) {                                      \
    bool r = false;                                                            \
    cb_invoke(CB_V_COUNT + CB_P1_COUNT + N, 1, true, &r, arg, CB_BP1);         \
    return r;                                                                  \
  }
#define DEF_CB_B(N)                                                            \
  static bool lua_cb_b_##N(void) {                                             \
    bool r = false;                                                            \
    cb_invoke(CB_V_COUNT + CB_P1_COUNT + CB_BP1_COUNT + N, 0, true, &r, NULL,  \
              CB_B);                                                           \
    return r;                                                                  \
  }

#define X(N) DEF_CB_V(N)
CB_V_SLOTS
#undef X
#define X(N) DEF_CB_P1(N)
CB_P1_SLOTS
#undef X
#define X(N) DEF_CB_BP1(N)
CB_BP1_SLOTS
#undef X
#define X(N) DEF_CB_B(N)
CB_B_SLOTS
#undef X

static void *cb_v_table[CB_V_COUNT] = {
#define X(N) lua_cb_v_##N,
    CB_V_SLOTS
#undef X
};
static void *cb_p1_table[CB_P1_COUNT] = {
#define X(N) lua_cb_p1_##N,
    CB_P1_SLOTS
#undef X
};
static void *cb_bp1_table[CB_BP1_COUNT] = {
#define X(N) lua_cb_bp1_##N,
    CB_BP1_SLOTS
#undef X
};
static void *cb_b_table[CB_B_COUNT] = {
#define X(N) lua_cb_b_##N,
    CB_B_SLOTS
#undef X
};

typedef struct {
  lua_State *L;
  const void *fn;
  int slot;
  int kind;
  LuaFeature *f;
} CbCacheEntry;

static CbCacheEntry cb_cache[CB_TOTAL];
static int cb_cache_count;

static void lua_cb_v_none(void) {}

static void lua_cb_p1_none(void *arg) { (void)arg; }

static bool lua_cb_bp1_none(void *arg) {
  (void)arg;
  return false;
}

static bool lua_cb_b_none(void) { return false; }

static void *cb_none_for(int kind) {
  if (kind == CB_V)
    return lua_cb_v_none;
  if (kind == CB_P1)
    return lua_cb_p1_none;
  if (kind == CB_BP1)
    return lua_cb_bp1_none;
  return lua_cb_b_none;
}

static void *cb_fn_for(int slot, int kind) {
  if (slot < 0 || slot >= CB_TOTAL)
    return cb_none_for(kind);
  if (kind == CB_V)
    return cb_v_table[slot];
  if (kind == CB_P1)
    return cb_p1_table[slot - CB_V_COUNT];
  if (kind == CB_BP1)
    return cb_bp1_table[slot - CB_V_COUNT - CB_P1_COUNT];
  return cb_b_table[slot - CB_V_COUNT - CB_P1_COUNT - CB_BP1_COUNT];
}

static void *make_callback(lua_State *L, int idx, int kind, LuaFeature *f) {
  static bool cb_init = false;
  if (!cb_init) {
    for (int i = 0; i < CB_TOTAL; i++)
      cb_refs[i] = LUA_NOREF;
    cb_init = true;
  }
  const void *key = lua_topointer(L, idx);
  for (int i = 0; i < cb_cache_count; i++) {
    if (cb_cache[i].L == L && cb_cache[i].fn == key && cb_cache[i].f == f &&
        cb_cache[i].kind == kind)
      return cb_fn_for(cb_cache[i].slot, kind);
  }

  int base = 0, count = 0;
  if (kind == CB_V) {
    base = 0;
    count = CB_V_COUNT;
  } else if (kind == CB_P1) {
    base = CB_V_COUNT;
    count = CB_P1_COUNT;
  } else if (kind == CB_BP1) {
    base = CB_V_COUNT + CB_P1_COUNT;
    count = CB_BP1_COUNT;
  } else {
    base = CB_V_COUNT + CB_P1_COUNT + CB_BP1_COUNT;
    count = CB_B_COUNT;
  }
  int slot = -1;
  for (int i = 0; i < count; i++)
    if (cb_refs[base + i] == LUA_NOREF && !cb_in_use[base + i]) {
      slot = base + i;
      break;
    }
  if (slot < 0) {
    log_error("lua callback slots exhausted (kind=%d)", kind);
    return cb_none_for(kind);
  }
  lua_pushvalue(L, idx);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  cb_refs[slot] = ref;
  cb_owners[slot] = f;
  track_ref(f, ref);
  if (cb_cache_count < CB_TOTAL) {
    CbCacheEntry *e = &cb_cache[cb_cache_count++];
    e->L = L;
    e->fn = key;
    e->slot = slot;
    e->kind = kind;
    e->f = f;
  }
  return cb_fn_for(slot, kind);
}

static void *lua_raw_ptr(lua_State *L, int idx) {
  if (lua_islightuserdata(L, idx))
    return lua_touserdata(L, idx);
  if (lua_isnumber(L, idx))
    return (void *)(uintptr_t)lua_tointeger(L, idx);
  if (lua_istable(L, idx)) {
    lua_getfield(L, idx, "__ptr");
    void *p = NULL;
    if (lua_islightuserdata(L, -1))
      p = lua_touserdata(L, -1);
    else if (lua_isnumber(L, -1))
      p = (void *)(uintptr_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return p;
  }
  return NULL;
}

void *slua_raw_ptr(void *L, int idx) {
  return lua_raw_ptr((lua_State *)L, idx);
}

static size_t aux_count_get(const void *base, const LuaFieldDesc *fd) {
  const char *p = (const char *)base + fd->aux;
  int bytes = fd->cb_kind ? fd->cb_kind : (int)sizeof(size_t);
  if (bytes == 4) {
    int n = *(const int *)p;
    return n > 0 ? (size_t)n : 0;
  }
  if (bytes == 2)
    return *(const unsigned short *)p;
  if (bytes == 1)
    return *(const unsigned char *)p;
  return *(const size_t *)p;
}

static void aux_count_set(void *base, const LuaFieldDesc *fd, size_t n) {
  char *p = (char *)base + fd->aux;
  int bytes = fd->cb_kind ? fd->cb_kind : (int)sizeof(size_t);
  if (bytes == 4)
    *(int *)p = n > INT_MAX ? INT_MAX : (int)n;
  else if (bytes == 2)
    *(unsigned short *)p = n > USHRT_MAX ? USHRT_MAX : (unsigned short)n;
  else if (bytes == 1)
    *(unsigned char *)p = n > UCHAR_MAX ? UCHAR_MAX : (unsigned char)n;
  else
    *(size_t *)p = n;
}

static void fill_struct_depth(lua_State *L, const char *sname, int idx,
                              void *base, LuaFeature *f, int depth) {
  if (depth > 4)
    return;
  const LuaStructDesc *desc = slua_struct_find(sname);
  if (!desc)
    luaL_error(L, "unsupported lua struct '%s'", sname);
  int base_idx = idx;
  if (idx < 0) {
    lua_pushvalue(L, idx);
    base_idx = lua_gettop(L);
  }
  for (int i = 0; i < desc->count; i++) {
    const LuaFieldDesc *fd = &desc->fields[i];
    lua_getfield(L, base_idx, fd->name);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      continue;
    }
    char *dst = (char *)base + fd->off;
    switch (fd->kind) {
    case LSF_INT:
      *(int *)dst = (int)lua_tointeger(L, -1);
      break;
    case LSF_BOOL:
      *(bool *)dst = lua_toboolean(L, -1) != 0;
      break;
    case LSF_FLOAT:
      *(float *)dst = (float)lua_tonumber(L, -1);
      break;
    case LSF_DOUBLE:
      *(double *)dst = lua_tonumber(L, -1);
      break;
    case LSF_STR: {
      size_t len = 0;
      const char *s = lua_tolstring(L, -1, &len);
      char *copy = (char *)calloc(len + 1, 1);
      *(const char **)dst = NULL;
      if (copy) {
        if (s)
          memcpy(copy, s, len);
        track_alloc(f, copy);
        *(const char **)dst = copy;
      }
      break;
    }
    case LSF_STR_INLINE: {
      size_t cap = fd->aux > 0 ? fd->aux : 1;
      size_t len = 0;
      const char *s2 = lua_tolstring(L, -1, &len);
      if (len >= cap)
        len = cap - 1;
      if (s2 && len)
        memcpy(dst, s2, len);
      dst[len] = '\0';
      break;
    }
    case LSF_STR_ARRAY: {
      size_t n = fd->aux > 0 ? fd->aux : 1;
      const char **arr = (const char **)calloc(n, sizeof(char *));
      if (!arr)
        break;
      if (lua_istable(L, -1)) {
        for (size_t ai = 0; ai < n; ai++) {
          lua_geti(L, -1, (lua_Integer)ai + 1);
          size_t len = 0;
          const char *s2 = lua_tolstring(L, -1, &len);
          char *copy = (char *)calloc(len + 1, 1);
          if (copy) {
            if (s2)
              memcpy(copy, s2, len);
            arr[ai] = copy;
            track_alloc(f, copy);
          }
          lua_pop(L, 1);
        }
      }
      track_alloc(f, arr);
      *(const char ***)dst = arr;
      break;
    }
    case LSF_STR_LIST: {
      size_t n = lua_istable(L, -1) ? (size_t)lua_rawlen(L, -1) : 0;
      char **arr = (char **)calloc(n + 1, sizeof(char *));
      if (!arr)
        break;
      for (size_t ai = 0; ai < n; ai++) {
        lua_geti(L, -1, (lua_Integer)(ai + 1));
        size_t len = 0;
        const char *s2 = lua_tolstring(L, -1, &len);
        char *copy = (char *)calloc(len + 1, 1);
        if (copy) {
          if (s2)
            memcpy(copy, s2, len);
          arr[ai] = copy;
          track_alloc(f, copy);
        }
        lua_pop(L, 1);
      }
      track_alloc(f, arr);
      *(char ***)dst = arr;
      aux_count_set(base, fd, n);
      break;
    }
    case LSF_BUFFER: {
      size_t len = 0;
      const char *s = lua_tolstring(L, -1, &len);
      char *buf = (char *)calloc(len + 1, 1);
      if (!buf)
        break;
      if (s)
        memcpy(buf, s, len);
      track_alloc(f, buf);
      *(char **)dst = buf;
      aux_count_set(base, fd, len);
      break;
    }
    case LSF_STRUCT_PTR: {
      void *raw = lua_raw_ptr(L, -1);
      if (raw) {
        *(void **)dst = raw;
        break;
      }
      if (lua_istable(L, -1)) {
        const LuaStructDesc *sd =
            fd->target ? slua_struct_find(fd->target) : NULL;
        if (sd) {
          void *child = calloc(1, sd->size);
          if (child) {
            track_alloc(f, child);
            fill_struct_depth(L, fd->target, -1, child, f, depth + 1);
            *(void **)dst = child;
          }
        }
      } else if (lua_islightuserdata(L, -1)) {
        *(void **)dst = lua_touserdata(L, -1);
      } else if (lua_isnumber(L, -1)) {
        *(void **)dst = (void *)(uintptr_t)lua_tointeger(L, -1);
      } else {
        *(void **)dst = NULL;
      }
      break;
    }
    case LSF_PTR:
      *(void **)dst = lua_raw_ptr(L, -1);
      break;
    case LSF_CB:
      *(void **)dst = make_callback(L, -1, fd->cb_kind, f);
      break;
    default:
      break;
    }
    lua_pop(L, 1);
  }
}

static void fill_struct(lua_State *L, const char *sname, int idx, void *base,
                        LuaFeature *f) {
  fill_struct_depth(L, sname, idx, base, f, 0);
}

static uintptr_t arg_value(lua_State *L, int idx) {
  if (lua_isstring(L, idx))
    return (uintptr_t)lua_tostring(L, idx);
  if (lua_isnumber(L, idx))
    return (uintptr_t)lua_tointeger(L, idx);
  if (lua_isboolean(L, idx))
    return (uintptr_t)(lua_toboolean(L, idx) != 0);
  if (lua_isuserdata(L, idx))
    return (uintptr_t)lua_touserdata(L, idx);
  return 0;
}

static void with_owner(lua_State *L, void (*fn)(lua_State *L)) {
  LuaFeature *f = lua_feature(L);
  const char *saved = slua_current_owner();
  char *copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f ? f->owner : NULL);
  fn(L);
  slua_owner_clear();
  if (copy) {
    slua_owner_set(copy);
    free(copy);
  }
}

static void call_v(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 2 + i);
  ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
             uintptr_t))sym)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushboolean(L, 1);
}

static void call_b(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 2 + i);
  bool r = ((bool (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                      uintptr_t))sym)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushboolean(L, r);
}

static void call_i(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 2 + i);
  int r = ((int (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                    uintptr_t))sym)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushinteger(L, r);
}

static void call_s(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 2 + i);
  char *r = ((char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                        uintptr_t))sym)(a[0], a[1], a[2], a[3], a[4], a[5]);
  if (r)
    lua_pushstring(L, r);
  else
    lua_pushnil(L);
}

static void call_p(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 2 + i);
  void *r = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                        uintptr_t))sym)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushlightuserdata(L, r);
}

static void call_f(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", name);
  double a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = lua_tonumber(L, 2 + i);
  double r = ((double (*)(double, double, double, double, double, double))sym)(
      a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushnumber(L, r);
}

static int l_v(lua_State *L) {
  with_owner(L, call_v);
  return 1;
}

static int l_b(lua_State *L) {
  with_owner(L, call_b);
  return 1;
}

static int l_i(lua_State *L) {
  with_owner(L, call_i);
  return 1;
}

static int l_s(lua_State *L) {
  with_owner(L, call_s);
  return 1;
}

static int l_f(lua_State *L) {
  with_owner(L, call_f);
  return 1;
}

static int l_p(lua_State *L) {
  with_owner(L, call_p);
  return 1;
}

static int l_feature(lua_State *L) {
  LuaFeature *f = lua_feature(L);
  lua_pushstring(L, f ? f->name : "");
  return 1;
}

static int l_source(lua_State *L) {
  LuaFeature *f = lua_feature(L);
  const char *path = f ? f->owner : "";
  const char *base = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\')
      base = p + 1;
  size_t n = strlen(base);
  if (n >= 4 && strcmp(base + n - 4, ".lua") == 0)
    n -= 4;
  char name[128] = {0};
  for (size_t i = 0; i < n && i + 1 < sizeof(name); i++) {
    unsigned char c = (unsigned char)base[i];
    if (isalnum(c) || c == '_' || c == '-')
      name[i] = (char)c;
    else
      name[i] = '_';
  }
  lua_pushstring(L, name);
  return 1;
}

static int l_owner(lua_State *L) {
  LuaFeature *f = lua_feature(L);
  lua_pushstring(L, f ? f->owner : "");
  return 1;
}

static int l_global_b(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  lua_pushboolean(L, sym && *(bool *)sym);
  return 1;
}

static int l_global_i(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol(name);
  lua_pushinteger(L, sym ? *(int *)sym : 0);
  return 1;
}

static int l_image_size(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int w = 0, h = 0;
  ski_get_image_size(path, &w, &h);
  lua_pushinteger(L, w);
  lua_pushinteger(L, h);
  return 2;
}

typedef struct {
  char *key;
  LuaFeature *owner;
  int ref;
  unsigned abi;
} LuaCap;

static LuaCap *g_lua_caps;
#define lua_cap_count arrlenu(g_lua_caps)

static LuaCap *lua_cap_find(const char *key) {
  if (!key || !key[0])
    return NULL;
  for (size_t i = 0; i < lua_cap_count; i++)
    if (g_lua_caps[i].key && strcmp(g_lua_caps[i].key, key) == 0)
      return &g_lua_caps[i];
  return NULL;
}

static void copy_lua_value(lua_State *from, int idx, lua_State *to, int depth) {
  idx = lua_absindex(from, idx);
  switch (lua_type(from, idx)) {
  case LUA_TNIL:
    lua_pushnil(to);
    break;
  case LUA_TBOOLEAN:
    lua_pushboolean(to, lua_toboolean(from, idx));
    break;
  case LUA_TNUMBER:
    if (lua_isinteger(from, idx))
      lua_pushinteger(to, lua_tointeger(from, idx));
    else
      lua_pushnumber(to, lua_tonumber(from, idx));
    break;
  case LUA_TSTRING: {
    size_t n = 0;
    const char *s = lua_tolstring(from, idx, &n);
    lua_pushlstring(to, s, n);
    break;
  }
  case LUA_TLIGHTUSERDATA:
    lua_pushlightuserdata(to, lua_touserdata(from, idx));
    break;
  case LUA_TTABLE:
    if (depth >= 8) {
      lua_pushnil(to);
      break;
    }
    int table_idx = lua_gettop(to);
    lua_newtable(to);
    lua_pushnil(from);
    while (lua_next(from, idx)) {
      copy_lua_value(from, -2, to, depth + 1);
      copy_lua_value(from, -1, to, depth + 1);
      lua_settable(to, table_idx);
      lua_pop(from, 1);
    }
    break;
  default:
    lua_pushnil(to);
    break;
  }
}

static int l_cap_register(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  unsigned abi = (unsigned)luaL_optinteger(L, 3, CAP_ABI_V1);
  LuaFeature *f = lua_feature(L);
  void *cb = make_callback(L, 2, CB_V, f);
  lua_pushvalue(L, 2);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  track_ref(f, ref);

  LuaCap *cap = lua_cap_find(key);
  if (cap) {
    LuaFeature *old = cap->owner;
    if (old && old->L && cap->ref != LUA_NOREF) {
      luaL_unref(old->L, LUA_REGISTRYINDEX, cap->ref);
      untrack_ref(old, cap->ref);
    }
    free(cap->key);
    cap->key = strdup(key);
    if (!cap->key)
      return luaL_error(L, "out of memory");
    cap->owner = f;
    cap->ref = ref;
    cap->abi = abi;
  } else {
    cap = arraddnptr(g_lua_caps, 1);
    cap->key = strdup(key);
    if (!cap->key)
      return luaL_error(L, "out of memory");
    cap->owner = f;
    cap->ref = ref;
    cap->abi = abi;
  }

  slua_owner_set(f ? f->owner : NULL);
  cap_register_abi(key, abi, cb);
  slua_owner_clear();
  return 0;
}

static void cap_declare_noop(void) {}

static int l_cap_bind_declare(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  LuaFeature *f = lua_feature(L);
  const char *saved = slua_current_owner();
  char *saved_copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f ? f->owner : NULL);
  cap_bind_declare(key, (void *)&cap_declare_noop);
  slua_owner_clear();
  if (saved_copy) {
    slua_owner_set(saved_copy);
    free(saved_copy);
  }
  return 0;
}

static int l_cap_call(lua_State *L) {
  const char *key = lua_tostring(L, lua_upvalueindex(1));
  unsigned abi = (unsigned)lua_tointeger(L, lua_upvalueindex(2));
  LuaCap *cap = lua_cap_find(key);
  if (cap && cap->owner && cap->owner->L && cap->ref != LUA_NOREF &&
      cap->abi == abi) {
    LuaFeature *f = cap->owner;
    int base = lua_gettop(f->L);
    lua_rawgeti(f->L, LUA_REGISTRYINDEX, cap->ref);
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; i++)
      copy_lua_value(L, i, f->L, 0);
    const char *saved = slua_current_owner();
    char *saved_copy = saved ? strdup(saved) : NULL;
    slua_owner_set(f->owner);
    int rc = lua_pcall(f->L, nargs, LUA_MULTRET, 0);
    slua_owner_clear();
    if (saved_copy) {
      slua_owner_set(saved_copy);
      free(saved_copy);
    }
    if (rc != 0) {
      const char *msg = lua_tostring(f->L, -1);
      log_error("lua capability error (%s): %s", key, msg ? msg : "unknown");
      lua_settop(f->L, base);
      lua_pushnil(L);
      return 1;
    }
    int nres = lua_gettop(f->L) - base;
    if (nres > 8)
      nres = 8;
    for (int i = 0; i < nres; i++)
      copy_lua_value(f->L, base + 1 + i, L, 0);
    lua_settop(f->L, base);
    return nres;
  }

  void *sym = Platform_GetMainSymbol("sk_cap_resolve_abi");
  if (!sym) {
    lua_pushboolean(L, 0);
    return 1;
  }
  void *target =
      ((void *(*)(const char *, unsigned))sym)(key ? key : "", abi);
  if (!target) {
    lua_pushboolean(L, 0);
    return 1;
  }
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, i + 1);
  uintptr_t r =
      ((uintptr_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                     uintptr_t))target)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushboolean(L, r != 0);
  return 1;
}

static int l_cap_resolve(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  unsigned abi = (unsigned)luaL_optinteger(L, 2, CAP_ABI_V1);
  LuaFeature *f = lua_feature(L);
  const char *saved = slua_current_owner();
  char *saved_copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f ? f->owner : NULL);
  cap_bind_declare(key, (void *)&l_cap_call);
  slua_owner_clear();
  if (saved_copy) {
    slua_owner_set(saved_copy);
    free(saved_copy);
  }
  lua_pushstring(L, key);
  lua_pushinteger(L, (lua_Integer)abi);
  lua_pushcclosure(L, l_cap_call, 2);
  return 1;
}

void slua_cap_purge_owner(const char *path_a, const char *path_b) {
  for (size_t i = 0; i < lua_cap_count;) {
    LuaCap *cap = &g_lua_caps[i];
    if (cap->owner && owner_module_matches(cap->owner->owner, path_a, path_b)) {
      if (cap->owner->L && cap->ref != LUA_NOREF) {
        luaL_unref(cap->owner->L, LUA_REGISTRYINDEX, cap->ref);
        untrack_ref(cap->owner, cap->ref);
      }
      free(cap->key);
      arrdelswap(g_lua_caps, (int)i);
      continue;
    }
    i++;
  }
}

static int l_struct_from_ptr(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *ptr = NULL;
  if (lua_islightuserdata(L, 2))
    ptr = lua_touserdata(L, 2);
  else if (lua_isnumber(L, 2))
    ptr = (void *)(uintptr_t)lua_tointeger(L, 2);
  if (!ptr) {
    lua_pushnil(L);
    return 1;
  }
  slua_push_struct(L, name, ptr);
  return 1;
}

static int l_struct_call(lua_State *L) {
  const char *fn = luaL_checkstring(L, 1);
  const char *sname = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);
  const char *ret = luaL_optstring(L, 4, "v");
  void *sym = Platform_GetMainSymbol(fn);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", fn);
  const LuaStructDesc *sd = slua_struct_find(sname);
  if (!sd || sd->size > 2048)
    luaL_error(L, "struct '%s' does not fit lua call storage", sname);
  char storage[2048] = {0};
  LuaFeature *f = lua_feature(L);
  const char *saved = slua_current_owner();
  char *copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f ? f->owner : NULL);
  fill_struct(L, sname, 3, storage, f);
  uintptr_t a[7] = {(uintptr_t)storage};
  for (int i = 0; i < 6; i++)
    a[i + 1] = arg_value(L, 5 + i);
  typedef uintptr_t (*generic_fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                  uintptr_t, uintptr_t, uintptr_t);
  uintptr_t r = ((generic_fn)sym)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
  slua_owner_clear();
  if (copy) {
    slua_owner_set(copy);
    free(copy);
  }
  if (strcmp(ret, "b") == 0)
    lua_pushboolean(L, r != 0);
  else if (strcmp(ret, "i") == 0)
    lua_pushinteger(L, (lua_Integer)r);
  else if (strcmp(ret, "s") == 0)
    lua_pushstring(L, r ? (const char *)r : "");
  else if (strcmp(ret, "f") == 0) {
    double d = 0;
    memcpy(&d, &r, sizeof(d));
    lua_pushnumber(L, d);
  } else if (strcmp(ret, "p") == 0)
    lua_pushlightuserdata(L, (void *)r);
  else
    lua_pushboolean(L, 1);
  return 1;
}

static int l_call_fn(lua_State *L) {
  const char *resolver = luaL_checkstring(L, 1);
  void *rs = Platform_GetMainSymbol(resolver);
  if (!rs)
    luaL_error(L, "unknown resolver '%s'", resolver);
  void *target = ((void *(*)(const char *))rs)(luaL_checkstring(L, 2));
  if (!target) {
    lua_pushboolean(L, 0);
    return 1;
  }
  uintptr_t a[6] = {0};
  for (int i = 0; i < 6; i++)
    a[i] = arg_value(L, 3 + i);
  uintptr_t r =
      ((uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                      uintptr_t))target)(a[0], a[1], a[2], a[3], a[4], a[5]);
  lua_pushboolean(L, r != 0);
  return 1;
}

static int l_fmt_call(lua_State *L) {
  const char *kind = luaL_checkstring(L, 1);
  const char *name = luaL_checkstring(L, 2);
  int n = lua_gettop(L) - 2;
  lua_getglobal(L, "string");
  lua_getfield(L, -1, "format");
  lua_remove(L, -2);
  for (int i = 3; i <= lua_gettop(L); i++)
    lua_pushvalue(L, i);
  lua_call(L, n, 1);
  const char *text = lua_tostring(L, -1);
  void *fn = Platform_GetMainSymbol(name);
  if (!fn)
    luaL_error(L, "unknown skapi function '%s'", name);
  uintptr_t a[6] = {(uintptr_t)text};
  uintptr_t r = 0;
  if (strcmp(kind, "f") == 0) {
    double d = ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                            uintptr_t, uintptr_t))fn)(a[0], 0, 0, 0, 0, 0);
    lua_pushnumber(L, d);
    lua_remove(L, -2);
    return 1;
  }
  if (strcmp(kind, "b") == 0)
    r = ((bool (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t))fn)(a[0], 0, 0, 0, 0, 0);
  else if (strcmp(kind, "i") == 0)
    r = (uintptr_t)((int (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                             uintptr_t, uintptr_t))fn)(a[0], 0, 0, 0, 0, 0);
  else if (strcmp(kind, "s") == 0) {
    char *out = ((char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                            uintptr_t, uintptr_t))fn)(a[0], 0, 0, 0, 0, 0);
    if (out)
      lua_pushstring(L, out);
    else
      lua_pushnil(L);
    lua_remove(L, -2);
    return 1;
  } else {
    ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
               uintptr_t))fn)(a[0], 0, 0, 0, 0, 0);
    lua_pushboolean(L, 1);
    return 1;
  }
  lua_pop(L, 1);
  if (strcmp(kind, "b") == 0)
    lua_pushboolean(L, r != 0);
  else
    lua_pushinteger(L, (lua_Integer)r);
  return 1;
}

static const luaL_Reg native_funcs[] = {
    {"v", l_v},
    {"b", l_b},
    {"i", l_i},
    {"s", l_s},
    {"f", l_f},
    {"p", l_p},
    {"feature", l_feature},
    {"owner", l_owner},
    {"source", l_source},
    {"global_b", l_global_b},
    {"global_i", l_global_i},
    {"image_size", l_image_size},
    {"cap_register", l_cap_register},
    {"cap_resolve", l_cap_resolve},
    {"cap_bind_declare", l_cap_bind_declare},
    {"call_fn", l_call_fn},
    {"fmt_call", l_fmt_call},
    {"struct_call", l_struct_call},
    {"struct_from_ptr", l_struct_from_ptr},
    {NULL, NULL},
};

char **slua_string_array(void *L, int idx) {
  lua_State *ls = (lua_State *)L;
  size_t n = lua_istable(ls, idx) ? (size_t)lua_rawlen(ls, idx) : 0;
  char **arr = (char **)calloc(n > 0 ? n : 1, sizeof(char *));
  if (!arr)
    return NULL;
  LuaFeature *f = lua_feature(ls);
  for (size_t i = 0; i < n; i++) {
    lua_geti(ls, idx, (lua_Integer)(i + 1));
    size_t len = 0;
    const char *s2 = lua_tolstring(ls, -1, &len);
    char *copy = (char *)calloc(len + 1, 1);
    if (copy) {
      if (s2)
        memcpy(copy, s2, len);
      arr[i] = copy;
      track_alloc(f, copy);
    }
    lua_pop(ls, 1);
  }
  track_alloc(f, arr);
  return arr;
}

void *slua_callback(void *L, int idx, int kind) {
  return make_callback((lua_State *)L, idx, kind, lua_feature((lua_State *)L));
}

void slua_fill_struct(void *L, const char *name, int idx, void *base) {
  fill_struct((lua_State *)L, name, idx, base, lua_feature((lua_State *)L));
}

static void push_struct_depth(lua_State *ls, const char *name, const void *base,
                              int depth) {
  if (depth > 4) {
    lua_pushnil(ls);
    return;
  }
  const LuaStructDesc *desc = slua_struct_find(name);
  if (!desc) {
    lua_pushnil(ls);
    return;
  }
  lua_newtable(ls);
  lua_pushliteral(ls, "__ptr");
  lua_pushlightuserdata(ls, (void *)base);
  lua_rawset(ls, -3);
  for (int i = 0; i < desc->count; i++) {
    const LuaFieldDesc *fd = &desc->fields[i];
    const char *p = (const char *)base + fd->off;
    switch (fd->kind) {
    case LSF_INT:
      lua_pushinteger(ls, (lua_Integer) * (int *)p);
      break;
    case LSF_BOOL:
      lua_pushboolean(ls, *(bool *)p != 0);
      break;
    case LSF_FLOAT:
      lua_pushnumber(ls, *(float *)p);
      break;
    case LSF_DOUBLE:
      lua_pushnumber(ls, *(double *)p);
      break;
    case LSF_STR:
      lua_pushstring(ls,
                     *(const char *const *)p ? *(const char *const *)p : "");
      break;
    case LSF_STR_INLINE:
      lua_pushstring(ls, (const char *)p);
      break;
    case LSF_STR_ARRAY: {
      lua_newtable(ls);
      const char *const *arr = *(const char *const *const *)p;
      size_t n = fd->aux;
      for (size_t ai = 0; arr && ai < n; ai++) {
        lua_pushinteger(ls, (lua_Integer)(ai + 1));
        lua_pushstring(ls, arr[ai] ? arr[ai] : "");
        lua_settable(ls, -3);
      }
      break;
    }
    case LSF_STR_LIST: {
      size_t n = aux_count_get(base, fd);
      lua_newtable(ls);
      char *const *arr = *(char *const *const *)p;
      for (size_t ai = 0; arr && ai < n; ai++) {
        lua_pushinteger(ls, (lua_Integer)(ai + 1));
        lua_pushstring(ls, arr[ai] ? arr[ai] : "");
        lua_settable(ls, -3);
      }
      break;
    }
    case LSF_BUFFER: {
      size_t n = aux_count_get(base, fd);
      lua_pushlstring(
          ls, *(const char *const *)p ? *(const char *const *)p : "", n);
      break;
    }
    case LSF_STRUCT_PTR: {
      void *child = *(void *const *)p;
      if (child && fd->target)
        push_struct_depth(ls, fd->target, child, depth + 1);
      else
        lua_pushnil(ls);
      break;
    }
    case LSF_PTR:
      lua_pushinteger(ls, (lua_Integer)(uintptr_t)*(void *const *)p);
      break;
    default:
      lua_pushnil(ls);
      break;
    }
    lua_setfield(ls, -2, fd->name);
  }
}

void slua_push_struct(void *L, const char *name, const void *base) {
  push_struct_depth((lua_State *)L, name, base, 0);
}

void slua_owner_from_state(void *L) {
  LuaFeature *f = lua_feature((lua_State *)L);
  slua_owner_set(f ? f->owner : NULL);
}

static int luaopen_skapi_native(lua_State *L) {
  luaL_newlib(L, native_funcs);
  slua_register_thunks(L);
  return 1;
}

static LuaFeature *find_feature(const char *name) {
  for (int i = 0; i < g_feature_count; i++)
    if (strcmp(g_features[i].name, name) == 0)
      return &g_features[i];
  return NULL;
}

static void feature_shutdown(LuaFeature *f) {
  if (!f || !f->L)
    return;
  lua_getglobal(f->L, "shutdown");
  if (lua_isfunction(f->L, -1)) {
    const char *saved = slua_current_owner();
    char *copy = saved ? strdup(saved) : NULL;
    slua_owner_set(f->owner);
    if (lua_pcall(f->L, 0, 0, 0) != 0)
      log_error("lua shutdown error: %s", lua_tostring(f->L, -1));
    slua_owner_clear();
    if (copy) {
      slua_owner_set(copy);
      free(copy);
    }
  } else {
    lua_pop(f->L, 1);
  }
  slua_cap_purge_owner(f->owner, NULL);
  for (int i = 0; i < CB_TOTAL; i++) {
    if (cb_owners[i] == f) {
      cb_refs[i] = LUA_NOREF;
      cb_owners[i] = NULL;
      cb_in_use[i] = false;
    }
  }
  for (int i = 0; i < cb_cache_count;) {
    if (cb_cache[i].f == f) {
      cb_cache[i] = cb_cache[--cb_cache_count];
      continue;
    }
    i++;
  }
  untrack_refs(f);
  for (int i = 0; i < f->alloc_count; i++)
    free(f->allocs[i]);
  free(f->allocs);
  f->allocs = NULL;
  f->alloc_count = 0;
  f->alloc_cap = 0;
  purge_owner_set(f->owner, NULL, widget_bump_generation());
  lua_close(f->L);
  f->L = NULL;
  free(f->name);
  f->name = NULL;
  free(f->owner);
  f->owner = NULL;
}

enum { CFG_LUA_BOOL, CFG_LUA_INT, CFG_LUA_FLOAT, CFG_LUA_STR, CFG_LUA_COLOR };

static void push_cfg_lua_global(lua_State *L, const char *name,
                                const void *value, int kind) {
  switch (kind) {
  case CFG_LUA_BOOL: {
    bool v;
    memcpy(&v, value, sizeof(v));
    lua_pushboolean(L, v != 0);
    break;
  }
  case CFG_LUA_INT: {
    int v;
    memcpy(&v, value, sizeof(v));
    lua_pushinteger(L, (lua_Integer)v);
    break;
  }
  case CFG_LUA_FLOAT: {
    float v;
    memcpy(&v, value, sizeof(v));
    lua_pushnumber(L, (lua_Number)v);
    break;
  }
  case CFG_LUA_STR: {
    const char *v;
    memcpy(&v, value, sizeof(v));
    lua_pushstring(L, v ? v : "");
    break;
  }
  default: {
    ColorRef v;
    memcpy(&v, value, sizeof(v));
    lua_pushinteger(L, (lua_Integer)v);
    break;
  }
  }
  lua_setglobal(L, name);
}

static void install_config_globals(lua_State *L) {
#define X(type, cname, def)                                                    \
  do {                                                                         \
    typeof(atomic_load(&cname)) v = atomic_load(&cname);                       \
    int kind = __builtin_types_compatible_p(typeof(v), bool)  ? CFG_LUA_BOOL   \
               : __builtin_types_compatible_p(typeof(v), int) ? CFG_LUA_INT    \
               : __builtin_types_compatible_p(typeof(v), float)                \
                   ? CFG_LUA_FLOAT                                             \
               : __builtin_types_compatible_p(typeof(v), const char *)         \
                   ? CFG_LUA_STR                                               \
                   : CFG_LUA_COLOR;                                            \
    push_cfg_lua_global(L, #cname, &v, kind);                                  \
  } while (0);
  CONFIG_VALUES(X)
#undef X
}

static bool feature_start(LuaFeature *f, const char *name, const char *path) {
  f->name = strdup(name);
  f->owner = strdup(path);
  f->L = luaL_newstate();
  if (!f->L)
    return false;
  luaL_openlibs(f->L);
  install_config_globals(f->L);
  const char *pkg = "package.path";
  lua_getglobal(f->L, "package");
  lua_getfield(f->L, -1, "path");
  const char *old = lua_tostring(f->L, -1);
  char newpath[1024];
  snprintf(newpath, sizeof(newpath), "src/?.lua;./src/?.lua;%s",
           old ? old : "");
  lua_pop(f->L, 1);
  lua_pushstring(f->L, newpath);
  lua_setfield(f->L, -2, "path");
  lua_pop(f->L, 1);
  (void)pkg;

  lua_pushlightuserdata(f->L, f);
  lua_setfield(f->L, LUA_REGISTRYINDEX, "skapi_feature");
  lua_getglobal(f->L, "package");
  lua_getfield(f->L, -1, "preload");
  lua_pushcfunction(f->L, luaopen_skapi_native);
  lua_setfield(f->L, -2, "skapi_native");
  lua_pop(f->L, 2);

  slua_owner_set(f->owner);
  int status = luaL_dofile(f->L, path);
  slua_owner_clear();
  if (status != 0) {
    log_error("lua feature %s failed: %s", name, lua_tostring(f->L, -1));
    feature_shutdown(f);
    return false;
  }
  return true;
}

bool slua_load(const char *name, const char *path) {
  if (!name || !path)
    return false;
  LuaFeature *f = find_feature(name);
  if (f && f->L)
    return true;
  if (!f) {
    if (g_feature_count >= g_feature_cap) {
      int cap = g_feature_cap > 0 ? g_feature_cap * 2 : 8;
      LuaFeature *grown =
          (LuaFeature *)realloc(g_features, (size_t)cap * sizeof(*g_features));
      if (!grown)
        return false;
      g_features = grown;
      g_feature_cap = cap;
    }
    f = &g_features[g_feature_count++];
    memset(f, 0, sizeof(*f));
  }
  return feature_start(f, name, path);
}

bool slua_reload(const char *name) {
  LuaFeature *f = find_feature(name);
  if (!f)
    return false;
  char *owner = f->owner ? strdup(f->owner) : NULL;
  char *path = owner;
  feature_shutdown(f);
  bool ok = feature_start(f, name, path);
  free(path);
  return ok;
}

void slua_shutdown(const char *name) {
  LuaFeature *f = find_feature(name);
  if (f)
    feature_shutdown(f);
}

bool slua_is_loaded(const char *name) {
  LuaFeature *f = find_feature(name);
  return f && f->L != NULL;
}
