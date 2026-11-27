#ifndef LUA_HOST_H
#define LUA_HOST_H

#include <stdbool.h>

bool slua_load(const char *name, const char *path);
bool slua_reload(const char *name);
void slua_shutdown(const char *name);
bool slua_is_loaded(const char *name);


const char *slua_current_owner(void);
void slua_owner_set(const char *owner);
void slua_owner_clear(void);
void slua_owner_from_state(void *L);
const char *slua_cb_struct_name(int kind);
void *slua_callback(void *L, int idx, int kind);
char **slua_string_array(void *L, int idx);
void slua_fill_struct(void *L, const char *name, int idx, void *base);
void slua_push_struct(void *L, const char *name, const void *base);

#endif
