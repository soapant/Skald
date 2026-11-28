#ifndef SNIPPET_MANAGER_H
#define SNIPPET_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "core/owner_registry.h"

typedef void (*SnippetFireFn)(int binding_index, int delete_count);

typedef struct {
  char *trigger;
  char *text;
  void (*func)(void);
  bool whitelist;
  char **windows;
  int window_count;
  OwnerTag tag;
} SnippetBinding;

typedef struct {
  const char *f_trigger;
  const char *f_text;
  void (*f_func)(void);
  bool f_whitelist;
  const char *const *f_windows;
  void *f_caller;
} SnippetRegisterInternal;

bool snippet_register_internal(SnippetRegisterInternal *argspec);
bool snippet_unregister_internal(const char *trigger);
void snippet_set_fire_fn(SnippetFireFn fn);
void snippet_fire(int binding_index, int delete_count);
bool snippet_feed_vk(int vk, int value, bool can_swallow);
void snippet_reset_buffer(void);
const SnippetBinding *snippet_get_binding(int index);
int snippet_binding_count(void);
void snippet_purge_owner(const char *path_a, const char *path_b);

#endif

