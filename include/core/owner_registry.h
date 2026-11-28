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
  void *f_arr;
  size_t f_elem_size;
  size_t f_tag_offset;
  const char *f_path_a;
  const char *f_path_b;
  unsigned f_max_gen;
  void (*f_destroy)(void *element);
} OwnerPurgeArray;

size_t owner_purge_array(OwnerPurgeArray *argspec);

typedef struct {
  void **f_arr;
  size_t f_tag_offset;
  const char *f_path_a;
  const char *f_path_b;
  unsigned f_max_gen;
  void (*f_destroy)(void *element);
} OwnerPurgePointers;

size_t owner_purge_pointers(OwnerPurgePointers *argspec);

typedef struct {
  const void *f_arr;
  size_t f_elem_size;
  size_t f_count;
  size_t f_tag_offset;
  const char *f_path;
} OwnerArrayHasActive;

bool owner_array_has_active(OwnerArrayHasActive *argspec);

bool owner_pointers_have_active(void *const *arr, size_t count,
                                size_t tag_offset, const char *path);

#define OWNER_ASSERT_STRUCT(arr, type)                                         \
  ((void)sizeof(char[_Generic((arr), type *: 1, default: -1)]))

#define OWNER_ASSERT_PTRS(arr, type)                                           \
  ((void)sizeof(char[_Generic((arr), type * *: 1, default: -1)]))

#define OWNER_PURGE_STRUCT(arr, type, path_a, path_b, max_gen, destroy)        \
  (OWNER_ASSERT_STRUCT((arr), type),                                           \
   owner_purge_array(&(OwnerPurgeArray){.f_arr = (arr), .f_elem_size = sizeof(type), .f_tag_offset = offsetof(type, tag), .f_path_a = (path_a), .f_path_b = (path_b), .f_max_gen = (max_gen), .f_destroy = (destroy)}))

#define OWNER_PURGE_PTRS(arr, type, path_a, path_b, max_gen, destroy)          \
  (OWNER_ASSERT_PTRS((arr), type),                                             \
   owner_purge_pointers(&(OwnerPurgePointers){.f_arr = (void **)(arr), .f_tag_offset = offsetof(type, tag), .f_path_a = (path_a), .f_path_b = (path_b), .f_max_gen = (max_gen), .f_destroy = (destroy)}))

#define OWNER_STRUCT_HAS_ACTIVE(arr, type, count, path)                        \
  (OWNER_ASSERT_STRUCT((arr), type),                                           \
   owner_array_has_active(&(OwnerArrayHasActive){.f_arr = (arr), .f_elem_size = sizeof(type), .f_count = (count), .f_tag_offset = offsetof(type, tag), .f_path = (path)}))

#define OWNER_PTRS_HAVE_ACTIVE(arr, type, count, path)                         \
  (OWNER_ASSERT_PTRS((arr), type),                                             \
   owner_pointers_have_active((void *const *)(arr), (count),                   \
                              offsetof(type, tag), (path)))

#endif

