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

size_t owner_purge_array(OwnerPurgeArray *argspec) {
  if (!argspec->arr || !argspec->elem_size ||
      owner_purge_inactive(argspec->path_a, argspec->path_b))
    return 0;
  char *a = owner_strdup(argspec->path_a);
  char *b = owner_strdup(argspec->path_b);
  size_t n = stbds_header(argspec->arr)->length;
  size_t write = 0;
  size_t removed = 0;
  char *base = (char *)argspec->arr;
  for (size_t i = 0; i < n; i++) {
    char *element = base + i * argspec->elem_size;
    OwnerTag *tag = (OwnerTag *)(element + argspec->tag_offset);
    if (owner_tag_should_purge(tag, a, b, argspec->max_gen)) {
      if (argspec->destroy)
        argspec->destroy(element);
      removed++;
      continue;
    }
    if (write != i)
      memmove(base + write * argspec->elem_size, element, argspec->elem_size);
    write++;
  }
  stbds_header(argspec->arr)->length = write;
  owner_strfree(&a);
  owner_strfree(&b);
  return removed;
}

size_t owner_purge_pointers(OwnerPurgePointers *argspec) {
  if (!argspec->arr || owner_purge_inactive(argspec->path_a, argspec->path_b))
    return 0;
  char *a = owner_strdup(argspec->path_a);
  char *b = owner_strdup(argspec->path_b);
  size_t n = stbds_header(argspec->arr)->length;
  size_t write = 0;
  size_t removed = 0;
  for (size_t i = 0; i < n; i++) {
    char *element = (char *)argspec->arr[i];
    OwnerTag *tag =
        element ? (OwnerTag *)(element + argspec->tag_offset) : NULL;
    if (owner_tag_should_purge(tag, a, b, argspec->max_gen)) {
      if (argspec->destroy)
        argspec->destroy(argspec->arr[i]);
      removed++;
      continue;
    }
    if (write != i)
      argspec->arr[write] = argspec->arr[i];
    write++;
  }
  stbds_header(argspec->arr)->length = write;
  owner_strfree(&a);
  owner_strfree(&b);
  return removed;
}

bool owner_array_has_active(OwnerArrayHasActive *argspec) {
  if (!argspec->arr || !argspec->path || !argspec->path[0])
    return false;
  const char *base = (const char *)argspec->arr;
  for (size_t i = 0; i < argspec->count; i++) {
    const OwnerTag *tag =
        (const OwnerTag *)(base + i * argspec->elem_size + argspec->tag_offset);
    if (owner_tag_active(tag, argspec->path))
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
