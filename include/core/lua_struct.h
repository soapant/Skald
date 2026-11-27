#ifndef LUA_STRUCT_H
#define LUA_STRUCT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  LSF_SKIP = 0,
  LSF_INT,
  LSF_BOOL,
  LSF_FLOAT,
  LSF_DOUBLE,
  LSF_STR,
  LSF_CB
} LuaFieldKind;

enum { LCB_V = 0, LCB_PS, LCB_BPS, LCB_B, LCB_COUNT };

typedef struct {
  const char *name;
  LuaFieldKind kind;
  size_t off;
  int cb_kind;
} LuaFieldDesc;

typedef struct {
  const char *name;
  size_t size;
  const LuaFieldDesc *fields;
  int count;
} LuaStructDesc;

#endif
