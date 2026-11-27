#include "core/lua_host.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/lua/lualib.h"

#include "core/skapi_lua_generated.h"
#include "core/owner_purge.h"
#include "core/window_manager.h"
#include "deps/log.h"
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

enum { CB_V, CB_PS, CB_BPS, CB_B };
#define CB_V_COUNT 32
#define CB_PS_COUNT 12
#define CB_BPS_COUNT 12
#define CB_B_COUNT 12
#define CB_TOTAL (CB_V_COUNT + CB_PS_COUNT + CB_BPS_COUNT + CB_B_COUNT)

static int cb_refs[CB_TOTAL];
static LuaFeature *cb_owners[CB_TOTAL];
static int cb_gen[CB_TOTAL];
static int cb_next_gen = 1;
static bool cb_in_use[CB_TOTAL];

static void cb_invoke(int slot, int nargs, bool want_bool, bool *out_bool) {
  LuaFeature *f = cb_owners[slot];
  if (!f || !f->L)
    return;
  cb_in_use[slot] = true;
  const char *saved = slua_current_owner();
  char *saved_copy = saved ? strdup(saved) : NULL;
  slua_owner_set(f->owner);
  lua_rawgeti(f->L, LUA_REGISTRYINDEX, cb_refs[slot]);
  if (nargs == 1)
    lua_pushlightuserdata(f->L, (void *)1);
  if (lua_pcall(f->L, nargs, want_bool ? 1 : 0, 0) != 0) {
    log_error("lua callback error: %s", lua_tostring(f->L, -1));
    lua_pop(f->L, 1);
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
  static void lua_cb_v_##N(void) { cb_invoke(N, 0, false, NULL); }
#define DEF_CB_PS(N)                                                           \
  static void lua_cb_ps_##N(PromptState *s) {                                 \
    (void)s;                                                                   \
    cb_invoke(CB_V_COUNT + N, 1, false, NULL);                                 \
  }
#define DEF_CB_BPS(N)                                                          \
  static bool lua_cb_bps_##N(PromptState *s) {                                \
    bool r = false;                                                            \
    (void)s;                                                                   \
    cb_invoke(CB_V_COUNT + CB_PS_COUNT + N, 1, true, &r);                      \
    return r;                                                                  \
  }
#define DEF_CB_B(N)                                                            \
  static bool lua_cb_b_##N(void) {                                            \
    bool r = false;                                                            \
    cb_invoke(CB_V_COUNT + CB_PS_COUNT + CB_BPS_COUNT + N, 0, true, &r);       \
    return r;                                                                  \
  }

DEF_CB_V(0) DEF_CB_V(1) DEF_CB_V(2) DEF_CB_V(3) DEF_CB_V(4) DEF_CB_V(5)
DEF_CB_V(6) DEF_CB_V(7) DEF_CB_V(8) DEF_CB_V(9) DEF_CB_V(10) DEF_CB_V(11)
DEF_CB_V(12) DEF_CB_V(13) DEF_CB_V(14) DEF_CB_V(15) DEF_CB_V(16) DEF_CB_V(17)
DEF_CB_V(18) DEF_CB_V(19) DEF_CB_V(20) DEF_CB_V(21) DEF_CB_V(22) DEF_CB_V(23)
DEF_CB_V(24) DEF_CB_V(25) DEF_CB_V(26) DEF_CB_V(27) DEF_CB_V(28) DEF_CB_V(29)
DEF_CB_V(30) DEF_CB_V(31)

DEF_CB_PS(0) DEF_CB_PS(1) DEF_CB_PS(2) DEF_CB_PS(3) DEF_CB_PS(4) DEF_CB_PS(5)
DEF_CB_PS(6) DEF_CB_PS(7) DEF_CB_PS(8) DEF_CB_PS(9) DEF_CB_PS(10) DEF_CB_PS(11)

DEF_CB_BPS(0) DEF_CB_BPS(1) DEF_CB_BPS(2) DEF_CB_BPS(3) DEF_CB_BPS(4)
DEF_CB_BPS(5) DEF_CB_BPS(6) DEF_CB_BPS(7) DEF_CB_BPS(8) DEF_CB_BPS(9)
DEF_CB_BPS(10) DEF_CB_BPS(11)

DEF_CB_B(0) DEF_CB_B(1) DEF_CB_B(2) DEF_CB_B(3) DEF_CB_B(4) DEF_CB_B(5)
DEF_CB_B(6) DEF_CB_B(7) DEF_CB_B(8) DEF_CB_B(9) DEF_CB_B(10) DEF_CB_B(11)

static void *cb_v_table[CB_V_COUNT] = {
    lua_cb_v_0,  lua_cb_v_1,  lua_cb_v_2,  lua_cb_v_3,  lua_cb_v_4,
    lua_cb_v_5,  lua_cb_v_6,  lua_cb_v_7,  lua_cb_v_8,  lua_cb_v_9,
    lua_cb_v_10, lua_cb_v_11, lua_cb_v_12, lua_cb_v_13, lua_cb_v_14,
    lua_cb_v_15, lua_cb_v_16, lua_cb_v_17, lua_cb_v_18, lua_cb_v_19,
    lua_cb_v_20, lua_cb_v_21, lua_cb_v_22, lua_cb_v_23, lua_cb_v_24,
    lua_cb_v_25, lua_cb_v_26, lua_cb_v_27, lua_cb_v_28, lua_cb_v_29,
    lua_cb_v_30, lua_cb_v_31};
static void *cb_ps_table[CB_PS_COUNT] = {
    lua_cb_ps_0, lua_cb_ps_1, lua_cb_ps_2,  lua_cb_ps_3,
    lua_cb_ps_4, lua_cb_ps_5, lua_cb_ps_6,  lua_cb_ps_7,
    lua_cb_ps_8, lua_cb_ps_9, lua_cb_ps_10, lua_cb_ps_11};
static void *cb_bps_table[CB_BPS_COUNT] = {
    lua_cb_bps_0, lua_cb_bps_1, lua_cb_bps_2,  lua_cb_bps_3,
    lua_cb_bps_4, lua_cb_bps_5, lua_cb_bps_6,  lua_cb_bps_7,
    lua_cb_bps_8, lua_cb_bps_9, lua_cb_bps_10, lua_cb_bps_11};
static void *cb_b_table[CB_B_COUNT] = {
    lua_cb_b_0, lua_cb_b_1, lua_cb_b_2,  lua_cb_b_3,
    lua_cb_b_4, lua_cb_b_5, lua_cb_b_6,  lua_cb_b_7,
    lua_cb_b_8, lua_cb_b_9, lua_cb_b_10, lua_cb_b_11};

typedef struct {
  lua_State *L;
  const void *fn;
  int slot;
  LuaFeature *f;
} CbCacheEntry;

static CbCacheEntry cb_cache[CB_TOTAL];
static int cb_cache_count;

static void *cb_fn_for(int slot, int kind) {
  if (kind == CB_V)
    return cb_v_table[slot];
  if (kind == CB_PS)
    return cb_ps_table[slot - CB_V_COUNT];
  if (kind == CB_BPS)
    return cb_bps_table[slot - CB_V_COUNT - CB_PS_COUNT];
  return cb_b_table[slot - CB_V_COUNT - CB_PS_COUNT - CB_BPS_COUNT];
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
    if (cb_cache[i].L == L && cb_cache[i].fn == key &&
        cb_cache[i].f == f)
      return cb_fn_for(cb_cache[i].slot, kind);
  }

  int base = 0, count = 0;
  if (kind == CB_V) {
    base = 0;
    count = CB_V_COUNT;
  } else if (kind == CB_PS) {
    base = CB_V_COUNT;
    count = CB_PS_COUNT;
  } else if (kind == CB_BPS) {
    base = CB_V_COUNT + CB_PS_COUNT;
    count = CB_BPS_COUNT;
  } else {
    base = CB_V_COUNT + CB_PS_COUNT + CB_BPS_COUNT;
    count = CB_B_COUNT;
  }
  int slot = -1;
  for (int i = 0; i < count; i++)
    if (cb_refs[base + i] == LUA_NOREF) {
      slot = base + i;
      break;
    }
  if (slot < 0) {
    for (int i = 0; i < count; i++) {
      int s = base + i;
      if (cb_in_use[s])
        continue;
      if (slot < 0 || cb_gen[s] < cb_gen[slot])
        slot = s;
    }
    if (slot < 0) {
      luaL_error(L, "no free lua callback slots");
      return NULL;
    }
    int old_ref = cb_refs[slot];
    LuaFeature *old_owner = cb_owners[slot];
    if (old_ref != LUA_NOREF && old_owner && old_owner->L)
      luaL_unref(old_owner->L, LUA_REGISTRYINDEX, old_ref);
    untrack_ref(old_owner, old_ref);
    for (int i = 0; i < cb_cache_count;) {
      if (cb_cache[i].slot == slot) {
        cb_cache[i] = cb_cache[--cb_cache_count];
        continue;
      }
      i++;
    }
  }
  cb_gen[slot] = cb_next_gen++;
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
    e->f = f;
  }
  return cb_fn_for(slot, kind);
}

static void fill_struct(lua_State *L, const char *sname, int idx, void *base,
                        LuaFeature *f) {
  const LuaStructDesc *desc = slua_struct_find(sname);
  if (!desc)
    luaL_error(L, "unsupported lua struct '%s'", sname);
  for (int i = 0; i < desc->count; i++) {
    const LuaFieldDesc *fd = &desc->fields[i];
    lua_getfield(L, idx, fd->name);
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
    case LSF_STR:
      *(const char **)dst = lua_tostring(L, -1);
      break;
    case LSF_STR_ARRAY: {
      size_t n = fd->aux > 0 ? fd->aux : 1;
      const char **arr = (const char **)calloc(n, sizeof(char *));
      if (!arr)
        break;
      if (lua_istable(L, -1)) {
        for (size_t ai = 0; ai < n; ai++) {
          lua_geti(L, -1, (lua_Integer)ai + 1);
          if (lua_isstring(L, -1))
            arr[ai] = lua_tostring(L, -1);
          lua_pop(L, 1);
        }
      }
      track_alloc(f, arr);
      *(const char ***)dst = arr;
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
      if (fd->aux)
        *(size_t *)((char *)base + fd->aux) = len;
      break;
    }
    case LSF_PTR:
      if (lua_islightuserdata(L, -1))
        *(void **)dst = lua_touserdata(L, -1);
      else if (lua_isnumber(L, -1))
        *(void **)dst = (void *)(uintptr_t)lua_tointeger(L, -1);
      else
        *(void **)dst = NULL;
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

static int l_feature(lua_State *L) {
  LuaFeature *f = lua_feature(L);
  lua_pushstring(L, f ? f->name : "");
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
static int l_image_size(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int w = 0, h = 0;
  ski_get_image_size(path, &w, &h);
  lua_pushinteger(L, w);
  lua_pushinteger(L, h);
  return 2;
}
static int l_cap_register(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  LuaFeature *f = lua_feature(L);
  void *cb = make_callback(L, 2, CB_V, f);
  slua_owner_set(f ? f->owner : NULL);
  ski_cap_register(key, cb);
  slua_owner_clear();
  return 0;
}
static int l_struct_call(lua_State *L) {
  const char *fn = luaL_checkstring(L, 1);
  const char *sname = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);
  const char *ret = luaL_optstring(L, 4, "v");
  void *sym = Platform_GetMainSymbol(fn);
  if (!sym)
    luaL_error(L, "unknown skapi function '%s'", fn);
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
  else if (strcmp(ret, "f") == 0) {
    double d = 0;
    memcpy(&d, &r, sizeof(d));
    lua_pushnumber(L, d);
  } else
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
  uintptr_t r = ((uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t))target)(
      a[0], a[1], a[2], a[3], a[4], a[5]);
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
    {"feature", l_feature},
    {"owner", l_owner},
    {"global_b", l_global_b},
    {"image_size", l_image_size},
    {"cap_register", l_cap_register},
    {"call_fn", l_call_fn},
    {"fmt_call", l_fmt_call},
    {"struct_call", l_struct_call},
    {NULL, NULL},
};

void *slua_callback(void *L, int idx, int kind) {
  return make_callback((lua_State *)L, idx, kind, lua_feature((lua_State *)L));
}

void slua_fill_struct(void *L, const char *name, int idx, void *base) {
  fill_struct((lua_State *)L, name, idx, base, lua_feature((lua_State *)L));
}

void slua_push_struct(void *L, const char *name, const void *base) {
  lua_State *ls = (lua_State *)L;
  const LuaStructDesc *desc = slua_struct_find(name);
  if (!desc) {
    lua_pushnil(ls);
    return;
  }
  lua_newtable(ls);
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
      lua_pushstring(ls, *(const char *const *)p ? *(const char *const *)p : "");
      break;
    default:
      lua_pushnil(ls);
      break;
    }
    lua_setfield(ls, -2, fd->name);
  }
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
  for (int i = 0; i < CB_TOTAL; i++) {
    if (cb_owners[i] == f) {
      cb_refs[i] = LUA_NOREF;
      cb_owners[i] = NULL;
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

static bool feature_start(LuaFeature *f, const char *name, const char *path) {
  f->name = strdup(name);
  f->owner = strdup(path);
  f->L = luaL_newstate();
  if (!f->L)
    return false;
  luaL_openlibs(f->L);
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
