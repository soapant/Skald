#ifndef SNIPPET_MANAGER_H
#define SNIPPET_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*SnippetFireFn)(int binding_index, int delete_count);

typedef struct {
  char *trigger;
  char *text;
  void (*func)(void);
  bool whitelist;
  char **windows;
  int window_count;
  char owner[512];
} SnippetBinding;

bool Snippet_RegisterInternal(const char *trigger, const char *text,
                              void (*func)(void), bool whitelist,
                              const char *const *windows, void *caller);
bool Snippet_UnregisterInternal(const char *trigger);
void Snippet_SetFireFn(SnippetFireFn fn);
bool Snippet_FeedVk(int vk_code, int value, bool can_swallow);
void Snippet_ResetBuffer(void);
const SnippetBinding *Snippet_GetBinding(int index);
int Snippet_BindingCount(void);
void Snippet_PurgeOwner(const char *path_a, const char *path_b);

#endif
