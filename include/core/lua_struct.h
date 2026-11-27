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
  LSF_STR_ARRAY,
  LSF_STR_LIST,
  LSF_STR_INLINE,
  LSF_STRUCT_PTR,
  LSF_BUFFER,
  LSF_PTR,
  LSF_CB
} LuaFieldKind;

enum { LCB_V = 0, LCB_P1, LCB_BP1, LCB_B, LCB_COUNT };

typedef struct {
  const char *name;
  LuaFieldKind kind;
  size_t off;
  size_t aux;
  int cb_kind;
  const char *target;
} LuaFieldDesc;

typedef struct {
  const char *name;
  size_t size;
  const LuaFieldDesc *fields;
  int count;
} LuaStructDesc;

#endif
