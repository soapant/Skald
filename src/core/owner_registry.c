#include "core/owner_registry.h"

#include <string.h>
#include "deps/stb_ds.h"

void owner_tag_set(OwnerTag *tag, const char *owner, unsigned gen) {
  if (!tag)
    return;
  owner_strfree(&tag->owner);
  tag->owner = owner_strdup(owner);
  tag->gen = gen;
}

void owner_tag_set_owned(OwnerTag *tag, char *owner, unsigned gen) {
  if (!tag)
    return;
  owner_strfree(&tag->owner);
  tag->owner = owner;
  tag->gen = gen;
}

void owner_tag_set_for_func(OwnerTag *tag, void *fn, unsigned gen) {
  if (!tag)
    return;
  owner_strfree(&tag->owner);
  tag->owner = owner_resolve_for_func_alloc(fn);
  tag->gen = gen;
}

void owner_tag_clear(OwnerTag *tag) {
  if (!tag)
    return;
  owner_strfree(&tag->owner);
  tag->gen = 0;
}

bool owner_tag_matches(const OwnerTag *tag, const char *path_a,
                       const char *path_b) {
  if (!tag)
    return false;
  return owner_module_matches(tag->owner, path_a, path_b);
}

bool owner_tag_active(const OwnerTag *tag, const char *path) {
  if (!tag || !path || !path[0])
    return false;
  return owner_module_matches(tag->owner, path, NULL);
}

bool owner_tag_should_purge(const OwnerTag *tag, const char *path_a,
                            const char *path_b, unsigned max_gen) {
  if (!tag || !tag->owner)
    return false;
  if (max_gen && tag->gen > max_gen)
    return false;
  return owner_module_matches(tag->owner, path_a, path_b);
}

size_t owner_purge_array(void *arr, size_t elem_size, size_t tag_offset,
                         const char *path_a, const char *path_b,
                         unsigned max_gen, void (*destroy)(void *element)) {
  if (!arr || !elem_size || owner_purge_inactive(path_a, path_b))
    return 0;
  char *a = owner_strdup(path_a);
  char *b = owner_strdup(path_b);
  size_t n = stbds_header(arr)->length;
  size_t write = 0;
  size_t removed = 0;
  char *base = (char *)arr;
  for (size_t i = 0; i < n; i++) {
    char *element = base + i * elem_size;
    OwnerTag *tag = (OwnerTag *)(element + tag_offset);
    if (owner_tag_should_purge(tag, a, b, max_gen)) {
      if (destroy)
        destroy(element);
      removed++;
      continue;
    }
    if (write != i)
      memmove(base + write * elem_size, element, elem_size);
    write++;
  }
  stbds_header(arr)->length = write;
  owner_strfree(&a);
  owner_strfree(&b);
  return removed;
}

size_t owner_purge_pointers(void **arr, size_t tag_offset,
                            const char *path_a, const char *path_b,
                            unsigned max_gen, void (*destroy)(void *element)) {
  if (!arr || owner_purge_inactive(path_a, path_b))
    return 0;
  char *a = owner_strdup(path_a);
  char *b = owner_strdup(path_b);
  size_t n = stbds_header(arr)->length;
  size_t write = 0;
  size_t removed = 0;
  for (size_t i = 0; i < n; i++) {
    char *element = (char *)arr[i];
    OwnerTag *tag = element ? (OwnerTag *)(element + tag_offset) : NULL;
    if (owner_tag_should_purge(tag, a, b, max_gen)) {
      if (destroy)
        destroy(arr[i]);
      removed++;
      continue;
    }
    if (write != i)
      arr[write] = arr[i];
    write++;
  }
  stbds_header(arr)->length = write;
  owner_strfree(&a);
  owner_strfree(&b);
  return removed;
}

bool owner_array_has_active(const void *arr, size_t elem_size, size_t count,
                            size_t tag_offset, const char *path) {
  if (!arr || !path || !path[0])
    return false;
  const char *base = (const char *)arr;
  for (size_t i = 0; i < count; i++) {
    const OwnerTag *tag =
        (const OwnerTag *)(base + i * elem_size + tag_offset);
    if (owner_tag_active(tag, path))
      return true;
  }
  return false;
}

bool owner_pointers_have_active(void *const *arr, size_t count,
                                size_t tag_offset, const char *path) {
  if (!arr || !path || !path[0])
    return false;
  for (size_t i = 0; i < count; i++) {
    const char *element = (const char *)arr[i];
    if (!element)
      continue;
    const OwnerTag *tag = (const OwnerTag *)(element + tag_offset);
    if (owner_tag_active(tag, path))
      return true;
  }
  return false;
}
