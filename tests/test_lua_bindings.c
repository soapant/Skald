#include "test.h"

#include <stdlib.h>

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
  ASSERT_TRUE(contains(lua, "native.s(\"sk_get_theme\""));
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

int main(void) {
  RUN(lua_bindings_generated_wrappers);
  RUN(lua_bindings_generated_structs);
  return skald_test_fails == 0 ? 0 : 1;
}
