#ifndef SNIPPET_MANAGER_H
#define SNIPPET_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*SnippetFireFn)(int binding_index, int delete_count);

typedef struct {
    char* trigger;
    char* text;
    void (*func)(void);
    bool whitelist;
    char** windows;
    int window_count;
    char owner[512];
} SnippetBinding;

bool snippet_register_internal(const char* trigger, const char* text, void (*func)(void), bool whitelist, const char** windows, void* caller);
void snippet_set_fire_fn(SnippetFireFn fn);
bool snippet_feed_vk(int vk_code, int value, bool can_swallow);
void snippet_reset_buffer(void);
const SnippetBinding* snippet_get_binding(int index);
int snippet_binding_count(void);
void snippet_purge_owner(const char* path_a, const char* path_b);

#endif
