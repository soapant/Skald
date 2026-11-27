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

size_t owner_purge_array(void *arr, size_t elem_size, size_t tag_offset,
                         const char *path_a, const char *path_b,
                         unsigned max_gen, void (*destroy)(void *element));

size_t owner_purge_pointers(void **arr, size_t tag_offset, const char *path_a,
                            const char *path_b, unsigned max_gen,
                            void (*destroy)(void *element));

bool owner_array_has_active(const void *arr, size_t elem_size, size_t count,
                            size_t tag_offset, const char *path);

bool owner_pointers_have_active(void *const *arr, size_t count,
                                size_t tag_offset, const char *path);

#define OWNER_ASSERT_STRUCT(arr, type)                                         \
  ((void)sizeof(char[_Generic((arr), type *: 1, default: -1)]))

#define OWNER_ASSERT_PTRS(arr, type)                                           \
  ((void)sizeof(char[_Generic((arr), type * *: 1, default: -1)]))

#define OWNER_PURGE_STRUCT(arr, type, path_a, path_b, max_gen, destroy)        \
  (OWNER_ASSERT_STRUCT((arr), type),                                           \
   owner_purge_array((arr), sizeof(type), offsetof(type, tag), (path_a),       \
                     (path_b), (max_gen), (destroy)))

#define OWNER_PURGE_PTRS(arr, type, path_a, path_b, max_gen, destroy)          \
  (OWNER_ASSERT_PTRS((arr), type),                                             \
   owner_purge_pointers((void **)(arr), offsetof(type, tag), (path_a),         \
                        (path_b), (max_gen), (destroy)))

#define OWNER_STRUCT_HAS_ACTIVE(arr, type, count, path)                        \
  (OWNER_ASSERT_STRUCT((arr), type),                                           \
   owner_array_has_active((arr), sizeof(type), (count), offsetof(type, tag),   \
                          (path)))

#define OWNER_PTRS_HAVE_ACTIVE(arr, type, count, path)                         \
  (OWNER_ASSERT_PTRS((arr), type),                                             \
   owner_pointers_have_active((void *const *)(arr), (count),                   \
                              offsetof(type, tag), (path)))

#endif
