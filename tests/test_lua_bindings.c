#include "test.h"

#include <stdlib.h>

#include "deps/lua/lauxlib.h"
#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"

static char *read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long n = ftell(f);
  if (n < 0) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[got] = '\0';
  return buf;
}

static bool contains(const char *haystack, const char *needle) {
  return haystack && strstr(haystack, needle) != NULL;
}

TEST(lua_bindings_generated_wrappers) {
  char *lua = read_text("src/skapi.lua");
  ASSERT_TRUE(lua);
  ASSERT_TRUE(contains(lua, "native.sk_get_theme"));
  ASSERT_TRUE(contains(lua, "native.sk_file_exists"));
  ASSERT_TRUE(contains(lua, "native.global_b(\"EnableBorders\")"));
  ASSERT_TRUE(contains(lua, "native.cap_resolve(key)"));
  ASSERT_TRUE(contains(lua, "native.struct_call(\"ski_place_checkbox\""));
  free(lua);
}

TEST(lua_bindings_generated_structs) {
  char *hdr = read_text("include/core/skapi_lua_generated.h");
  ASSERT_TRUE(hdr);
  ASSERT_TRUE(contains(hdr, "{\"x\", LSF_INT, offsetof(DrawRect, x)"));
  ASSERT_TRUE(contains(hdr, "{\"y\", LSF_INT, offsetof(DrawRect, y)"));
  ASSERT_TRUE(contains(hdr, "{\"w\", LSF_INT, offsetof(DrawRect, w)"));
  ASSERT_TRUE(contains(hdr, "{\"h\", LSF_INT, offsetof(DrawRect, h)"));
  ASSERT_TRUE(contains(hdr, "LSF_STR_ARRAY"));
  ASSERT_TRUE(contains(hdr, "offsetof(SkHotkeySpec, whitelist)"));
  ASSERT_TRUE(contains(hdr,
                       "{\"items\", LSF_STR_LIST, offsetof(ListBox, items), "
                       "offsetof(ListBox, item_count), 4, NULL}"));
  ASSERT_TRUE(contains(hdr, "lua_alias_from[]"));
  ASSERT_TRUE(contains(hdr, "\"SkRect\""));
  ASSERT_TRUE(contains(hdr, "\"DrawRect\""));
  ASSERT_TRUE(contains(hdr, "ListBox *a0 = (ListBox *)slua_raw_ptr(L, 1);"));
  free(hdr);
}

static lua_State *binding_lua = NULL;

static void close_binding_lua(void) {
  if (binding_lua) {
    lua_close(binding_lua);
    binding_lua = NULL;
  }
}

static void push_nth_call(lua_State *L, int n) {
  lua_getglobal(L, "__calls");
  lua_rawgeti(L, -1, n);
  lua_remove(L, -2);
}

static const char *call_string(lua_State *L, const char *field) {
  lua_getfield(L, -1, field);
  const char *value = lua_tostring(L, -1);
  lua_pop(L, 1);
  return value;
}

static lua_Number call_number(lua_State *L, const char *field) {
  lua_getfield(L, -1, field);
  lua_Number value = lua_tonumber(L, -1);
  lua_pop(L, 1);
  return value;
}

TEST(lua_bindings_runtime_wrappers) {
  TEST_TEARDOWN(close_binding_lua);
  binding_lua = luaL_newstate();
  ASSERT_TRUE(binding_lua);
  luaL_openlibs(binding_lua);

  const char *script =
      "package.path = 'src/?.lua;' .. package.path\n"
      "__calls = {}\n"
      "local function record(name)\n"
      "  return function(...)\n"
      "    table.insert(__calls, {name = name, n = select('#', ...),\n"
      "                           first = (...) or false})\n"
      "    return true\n"
      "  end\n"
      "end\n"
      "package.preload['skapi_native'] = function()\n"
      "  local native = {}\n"
      "  native.struct_call = function(fn, sname, t, ret, ...)\n"
      "    table.insert(__calls, {name = 'struct_call', fn = fn,\n"
      "                           sname = sname, ret = ret,\n"
      "                           n = select('#', ...),\n"
      "                           first = (...) or false,\n"
      "                           orientation = t.orientation})\n"
      "    return true\n"
      "  end\n"
      "  native.global_b = function() return false end\n"
      "  native.global_i = function() return 0 end\n"
      "  native.source = function() return 'runtime' end\n"
      "  native.owner = function() return 'runtime' end\n"
      "  setmetatable(native, {__index = function(_, k) return record(k) "
      "end})\n"
      "  return native\n"
      "end\n"
      "local sk = require('skapi')\n"
      "sk.window_open('title')\n"
      "sk.key_pressed(1)\n"
      "sk.key_released(2)\n"
      "sk.place_slider({})\n"
      "sk.place_vslider({})\n"
      "sk.set_window_geometry({}, 42)\n";

  int status = luaL_dostring(binding_lua, script);
  if (status != 0)
    printf("  lua error: %s\n", lua_tostring(binding_lua, -1));
  ASSERT_EQ_INT(status, 0);

  lua_getglobal(binding_lua, "__calls");
  int count = (int)lua_rawlen(binding_lua, -1);
  lua_pop(binding_lua, 1);
  ASSERT_EQ_INT(count, 6);

  push_nth_call(binding_lua, 1);
  ASSERT_EQ_STR(call_string(binding_lua, "name"), "sk_is_window_open");
  lua_pop(binding_lua, 1);

  push_nth_call(binding_lua, 2);
  ASSERT_EQ_STR(call_string(binding_lua, "name"), "sk_is_key_pressed");
  lua_pop(binding_lua, 1);

  push_nth_call(binding_lua, 3);
  ASSERT_EQ_STR(call_string(binding_lua, "name"), "sk_is_key_released");
  lua_pop(binding_lua, 1);

  push_nth_call(binding_lua, 4);
  ASSERT_EQ_STR(call_string(binding_lua, "fn"), "ski_place_slider");
  ASSERT_EQ_INT(call_number(binding_lua, "orientation"), 0);
  lua_pop(binding_lua, 1);

  push_nth_call(binding_lua, 5);
  ASSERT_EQ_STR(call_string(binding_lua, "fn"), "ski_place_slider");
  ASSERT_EQ_INT(call_number(binding_lua, "orientation"), 1);
  lua_pop(binding_lua, 1);

  push_nth_call(binding_lua, 6);
  ASSERT_EQ_STR(call_string(binding_lua, "fn"), "ski_set_window_geometry");
  ASSERT_EQ_INT(call_number(binding_lua, "n"), 1);
  ASSERT_EQ_INT(call_number(binding_lua, "first"), 42);
  lua_pop(binding_lua, 1);
}

int main(void) {
  RUN(lua_bindings_generated_wrappers);
  RUN(lua_bindings_generated_structs);
  RUN(lua_bindings_runtime_wrappers);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
