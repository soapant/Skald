#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "platform/platform.h"
#include "core/types/keys.h"
#include "deps/log.h"
#include "helpers/helpers_owner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNIPPET_MAX_BINDINGS 64
#define SNIPPET_BUF_CAP 128

static SnippetBinding g_snippets[SNIPPET_MAX_BINDINGS];
static int g_snippet_count = 0;
static SnippetFireFn g_fire_fn = NULL;

static char g_buf[SNIPPET_BUF_CAP];
static int g_buf_len = 0;

void snippet_set_fire_fn(SnippetFireFn fn) {
    g_fire_fn = fn;
}

void snippet_reset_buffer(void) {
    g_buf_len = 0;
}

const SnippetBinding* snippet_get_binding(int index) {
    if (index < 0 || index >= g_snippet_count) return NULL;
    return &g_snippets[index];
}

int snippet_binding_count(void) {
    return g_snippet_count;
}

static void snippet_free_binding(SnippetBinding* b) {
    if (!b) return;
    free(b->trigger);
    free(b->text);
    for (int j = 0; j < b->window_count; j++) free(b->windows[j]);
    free(b->windows);
    b->trigger = NULL;
    b->text = NULL;
    b->windows = NULL;
    b->window_count = 0;
}

bool snippet_register_internal(const char* trigger, const char* text, void (*func)(void), bool whitelist, const char** windows, void* caller) {
    if (!trigger || !trigger[0]) return false;
    if (!text && !func) return false;

    char owner[OWNER_PATH_MAX];
    owner[0] = '\0';
    if (func) Owner_ResolveForFunc((void*)(uintptr_t)func, owner, sizeof(owner));
    if (!owner[0] && caller) Owner_ResolveForFunc(caller, owner, sizeof(owner));

    for (int i = 0; i < g_snippet_count; i++) {
        if (g_snippets[i].trigger && strcmp(g_snippets[i].trigger, trigger) == 0) {
            char* nt = strdup(trigger);
            if (!nt) return false;
            snippet_free_binding(&g_snippets[i]);
            g_snippets[i].trigger = nt;
            g_snippets[i].text = text ? strdup(text) : NULL;
            g_snippets[i].func = func;
            g_snippets[i].whitelist = whitelist;
            g_snippets[i].windows = NULL;
            g_snippets[i].window_count = 0;
            snprintf(g_snippets[i].owner, sizeof(g_snippets[i].owner), "%s", owner);
            if (windows) {
                int n = 0;
                while (windows[n]) n++;
                if (n > 0) {
                    g_snippets[i].windows = malloc((size_t)n * sizeof(char*));
                    if (!g_snippets[i].windows) return false;
                    for (int j = 0; j < n; j++) {
                        g_snippets[i].windows[j] = strdup(windows[j] ? windows[j] : "");
                        if (!g_snippets[i].windows[j]) return false;
                    }
                    g_snippets[i].window_count = n;
                }
            }
            snippet_reset_buffer();
            return true;
        }
    }
    if (g_snippet_count >= SNIPPET_MAX_BINDINGS) return false;
    SnippetBinding* b = &g_snippets[g_snippet_count];
    memset(b, 0, sizeof(*b));
    b->trigger = strdup(trigger);
    if (!b->trigger) return false;
    b->text = text ? strdup(text) : NULL;
    if (text && !b->text) { free(b->trigger); return false; }
    b->func = func;
    b->whitelist = whitelist;
    b->windows = NULL;
    b->window_count = 0;
    snprintf(b->owner, sizeof(b->owner), "%s", owner);
    if (windows) {
        int n = 0;
        while (windows[n]) n++;
        if (n > 0) {
            b->windows = malloc((size_t)n * sizeof(char*));
            if (!b->windows) { free(b->trigger); free(b->text); return false; }
            for (int j = 0; j < n; j++) {
                b->windows[j] = strdup(windows[j] ? windows[j] : "");
                if (!b->windows[j]) {
                    for (int k = 0; k < j; k++) free(b->windows[k]);
                    free(b->windows);
                    free(b->trigger);
                    free(b->text);
                    return false;
                }
            }
            b->window_count = n;
        }
    }
    g_snippet_count++;
    snippet_reset_buffer();
    return true;
}

void snippet_purge_owner(const char* path_a, const char* path_b) {
    if ((!path_a || !path_a[0]) && (!path_b || !path_b[0])) return;

    int n = 0;
    for (int i = 0; i < g_snippet_count; i++) {
        if (Owner_ModuleMatches(g_snippets[i].owner, path_a, path_b)) {
            log_info("snippet '%s' purged (owner unloaded)", g_snippets[i].trigger ? g_snippets[i].trigger : "?");
            snippet_free_binding(&g_snippets[i]);
            continue;
        }
        if (n != i) g_snippets[n] = g_snippets[i];
        n++;
    }
    int dropped = g_snippet_count - n;
    if (dropped > 0) {
        memset(&g_snippets[n], 0, (size_t)(g_snippet_count - n) * sizeof(SnippetBinding));
        g_snippet_count = n;
        log_info("snippet purged %d binding%s", dropped, dropped == 1 ? "" : "s");
    }
}

static bool snippet_window_allowed(const SnippetBinding* b, const char* title) {
    if (!b) return true;
    if (b->window_count == 0) return !b->whitelist;
    if (!b->whitelist) {
        for (int j = 0; j < b->window_count; j++) {
            if (b->windows[j] && b->windows[j][0] && Platform_StringContainsCaseInsensitive(title, b->windows[j])) return false;
        }
        return true;
    }
    if (!title) return false;
    for (int j = 0; j < b->window_count; j++) {
        if (b->windows[j] && b->windows[j][0] && Platform_StringContainsCaseInsensitive(title, b->windows[j])) return true;
    }
    return false;
}

static char snippet_char_for_vk(int vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return shift ? (char)vk : (char)(vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') {
        if (!shift) return (char)vk;
        switch (vk) {
            case '1': return '!';
            case '2': return '@';
            case '3': return '#';
            case '4': return '$';
            case '5': return '%';
            case '6': return '^';
            case '7': return '&';
            case '8': return '*';
            case '9': return '(';
            case '0': return ')';
        }
        return 0;
    }
    if (vk == VK_SPACE) return ' ';
    switch (vk) {
        case VK_OEM_MINUS: return shift ? '_' : '-';
        case VK_OEM_PLUS: return shift ? '+' : '=';
        case VK_OEM_4: return shift ? '{' : '[';
        case VK_OEM_6: return shift ? '}' : ']';
        case VK_OEM_1: return shift ? ':' : ';';
        case VK_OEM_7: return shift ? '"' : '\'';
        case VK_OEM_COMMA: return shift ? '<' : ',';
        case VK_OEM_PERIOD: return shift ? '>' : '.';
        case VK_OEM_2: return shift ? '?' : '/';
        case VK_OEM_3: return shift ? '~' : '`';
    }
    return 0;
}

static void snippet_push_char(char c) {
    if (g_buf_len >= SNIPPET_BUF_CAP) {
        memmove(g_buf, g_buf + 1, SNIPPET_BUF_CAP - 1);
        g_buf_len = SNIPPET_BUF_CAP - 1;
    }
    g_buf[g_buf_len++] = c;
}

static int snippet_match(void) {
    for (int i = 0; i < g_snippet_count; i++) {
        if (!g_snippets[i].trigger || !g_snippets[i].trigger[0]) continue;
        size_t tlen = strlen(g_snippets[i].trigger);
        if ((int)tlen > g_buf_len) continue;
        if (memcmp(g_buf + g_buf_len - (int)tlen, g_snippets[i].trigger, tlen) == 0) return i;
    }
    return -1;
}

bool snippet_feed_vk(int vk_code, int value, bool can_swallow) {
    if (value == 0) return false;
    if (vk_code == VK_BACK) {
        if (g_buf_len > 0) g_buf_len--;
        return false;
    }
    if (vk_code == VK_RETURN || vk_code == VK_ESCAPE) {
        snippet_reset_buffer();
        return false;
    }
    int mods = remap_current_mods();
    if (mods & (HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT | HOTKEY_MOD_SUPER)) return false;
    char c = snippet_char_for_vk(vk_code, (mods & HOTKEY_MOD_SHIFT) != 0);
    if (!c) return false;
    snippet_push_char(c);
    int idx = snippet_match();
    if (idx < 0) return false;
    const char* title = NULL;
    if (g_snippets[idx].window_count > 0) title = Platform_GetActiveWindowTitle();
    if (!snippet_window_allowed(&g_snippets[idx], title)) return false;
    size_t tlen = strlen(g_snippets[idx].trigger);
    int del = (int)tlen - (can_swallow ? 1 : 0);
    if (del < 0) del = 0;
    snippet_reset_buffer();
    if (g_fire_fn) g_fire_fn(idx, del);
    return true;
}
