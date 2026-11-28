#ifndef OWNER_REGISTRY_H
#define OWNER_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include "helpers/helpers_owner.h"

typedef struct {
  char *owner;
  unsigned gen;
} OwnerTag;

void owner_tag_set(OwnerTag *tag, const char *owner, unsigned gen);
void owner_tag_set_owned(OwnerTag *tag, char *owner, unsigned gen);
void owner_tag_set_for_func(OwnerTag *tag, void *fn, unsigned gen);
void owner_tag_clear(OwnerTag *tag);
bool owner_tag_matches(const OwnerTag *tag, const char *path_a,
                       const char *path_b);
bool owner_tag_active(const OwnerTag *tag, const char *path);
bool owner_tag_should_purge(const OwnerTag *tag, const char *path_a,
                            const char *path_b, unsigned max_gen);

typedef struct {
  void *arr;
  size_t elem_size;
  size_t tag_offset;
  const char *path_a;
  const char *path_b;
  unsigned max_gen;
  void (*destroy)(void *element);
} OwnerPurgeArray;

size_t owner_purge_array(OwnerPurgeArray *argspec);

typedef struct {
  void **arr;
  size_t tag_offset;
  const char *path_a;
  const char *path_b;
  unsigned max_gen;
  void (*destroy)(void *element);
} OwnerPurgePointers;

size_t owner_purge_pointers(OwnerPurgePointers *argspec);

typedef struct {
  const void *arr;
  size_t elem_size;
  size_t count;
  size_t tag_offset;
  const char *path;
} OwnerArrayHasActive;

bool owner_array_has_active(OwnerArrayHasActive *argspec);

bool owner_pointers_have_active(void *const *arr, size_t count,
                                size_t tag_offset, const char *path);

#define OWNER_ASSERT_STRUCT(f_arr, f_type)                                     \
  ((void)sizeof(char[_Generic((f_arr), f_type *: 1, default: -1)]))

#define OWNER_ASSERT_PTRS(f_arr, f_type)                                       \
  ((void)sizeof(char[_Generic((f_arr), f_type * *: 1, default: -1)]))

#define OWNER_PURGE_STRUCT(f_arr, f_type, f_path_a, f_path_b, f_max_gen,       \
                           f_destroy)                                          \
  (OWNER_ASSERT_STRUCT((f_arr), f_type),                                       \
   owner_purge_array(&(OwnerPurgeArray){.arr = (f_arr),                        \
                                        .elem_size = sizeof(f_type),           \
                                        .tag_offset = offsetof(f_type, tag),   \
                                        .path_a = (f_path_a),                  \
                                        .path_b = (f_path_b),                  \
                                        .max_gen = (f_max_gen),                \
                                        .destroy = (f_destroy)}))

#define OWNER_PURGE_PTRS(f_arr, f_type, f_path_a, f_path_b, f_max_gen,         \
                         f_destroy)                                            \
  (OWNER_ASSERT_PTRS((f_arr), f_type),                                         \
   owner_purge_pointers(                                                       \
       &(OwnerPurgePointers){.arr = (void **)(f_arr),                          \
                             .tag_offset = offsetof(f_type, tag),              \
                             .path_a = (f_path_a),                             \
                             .path_b = (f_path_b),                             \
                             .max_gen = (f_max_gen),                           \
                             .destroy = (f_destroy)}))

#define OWNER_STRUCT_HAS_ACTIVE(f_arr, f_type, f_count, f_path)                \
  (OWNER_ASSERT_STRUCT((f_arr), f_type),                                       \
   owner_array_has_active(                                                     \
       &(OwnerArrayHasActive){.arr = (f_arr),                                  \
                              .elem_size = sizeof(f_type),                     \
                              .count = (f_count),                              \
                              .tag_offset = offsetof(f_type, tag),             \
                              .path = (f_path)}))

#define OWNER_PTRS_HAVE_ACTIVE(f_arr, f_type, f_count, f_path)                 \
  (OWNER_ASSERT_PTRS((f_arr), f_type),                                         \
   owner_pointers_have_active((void *const *)(f_arr), (f_count),               \
                              offsetof(f_type, tag), (f_path)))

#endif
