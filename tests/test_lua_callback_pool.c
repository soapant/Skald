#include "test.h"

#include <stdlib.h>

#include "deps/lua/lauxlib.h"
#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"

#include "core/lua_host.h"

static lua_State *pool_lua = NULL;

static void close_pool(void) {
  if (pool_lua) {
    lua_close(pool_lua);
    pool_lua = NULL;
  }
}

static int lua_noop_a(lua_State *L) {
  (void)L;
  return 0;
}

static int lua_noop_b(lua_State *L) {
  (void)L;
  return 0;
}

TEST(callback_pool_caches_same_function_and_kind) {
  TEST_TEARDOWN(close_pool);
  pool_lua = luaL_newstate();
  ASSERT_TRUE(pool_lua);

  lua_pushcfunction(pool_lua, lua_noop_a);
  void *first = slua_callback(pool_lua, -1, 0);
  ASSERT_TRUE(first != NULL);
  void *again = slua_callback(pool_lua, -1, 0);
  ASSERT_TRUE(first == again);
  lua_pop(pool_lua, 1);
}

TEST(callback_pool_allocates_distinct_slots) {
  TEST_TEARDOWN(close_pool);
  pool_lua = luaL_newstate();
  ASSERT_TRUE(pool_lua);

  lua_pushcfunction(pool_lua, lua_noop_a);
  void *a = slua_callback(pool_lua, -1, 0);
  void *a_other_kind = slua_callback(pool_lua, -1, 1);
  lua_pop(pool_lua, 1);
  ASSERT_TRUE(a != NULL);
  ASSERT_TRUE(a_other_kind != NULL);
  ASSERT_TRUE(a != a_other_kind);

  lua_pushcfunction(pool_lua, lua_noop_b);
  void *b = slua_callback(pool_lua, -1, 0);
  lua_pop(pool_lua, 1);
  ASSERT_TRUE(b != NULL);
  ASSERT_TRUE(b != a);
}

TEST(callback_struct_name_mapping) {
  ASSERT_EQ_STR(slua_cb_struct_name(1), "PromptState");
  ASSERT_EQ_STR(slua_cb_struct_name(2), "PromptState");
  ASSERT_TRUE(slua_cb_struct_name(0) == NULL);
}

TEST(lua_owner_tls_roundtrip) {
  slua_owner_clear();
  ASSERT_TRUE(slua_current_owner() == NULL);
  slua_owner_set("/x/probe.so");
  ASSERT_EQ_STR(slua_current_owner(), "/x/probe.so");
  slua_owner_clear();
  ASSERT_TRUE(slua_current_owner() == NULL);
}

int main(void) {
  RUN(callback_pool_caches_same_function_and_kind);
  RUN(callback_pool_allocates_distinct_slots);
  RUN(callback_struct_name_mapping);
  RUN(lua_owner_tls_roundtrip);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
