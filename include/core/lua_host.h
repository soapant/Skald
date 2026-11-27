#ifndef LUA_HOST_H
#define LUA_HOST_H

#include <stdbool.h>

bool ski_lua_load(const char *name, const char *path);
bool ski_lua_reload(const char *name);
void ski_lua_shutdown(const char *name);
bool ski_lua_is_loaded(const char *name);

const char *ski_lua_current_owner(void);
void ski_lua_owner_set(const char *owner);
void ski_lua_owner_clear(void);

#endif
