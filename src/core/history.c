#include "core/history.h"
#include "deps/log.h"
#include "core/hashmap.h"
#include "core/context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct {
    char** items;
    int count;
    int capacity;
    int cursor;
} HistoryList;

static HashMap* g_history = NULL;

static void free_history_list(void* v) {
    HistoryList* hl = (HistoryList*)v;
    if (!hl) return;
    for (int i = 0; i < hl->count; i++) free(hl->items[i]);
    free(hl->items);
    free(hl);
}

static HistoryList* get_or_create(const char* feature) {
    if (!feature || !feature[0]) return NULL;
    if (!g_history) g_history = HashMap_Create(32);
    HistoryList* hl = (HistoryList*)HashMap_Get(g_history, feature);
    if (!hl) {
        hl = calloc(1, sizeof(HistoryList));
        if (!hl) return NULL;
        hl->capacity = 16;
        hl->items = calloc(hl->capacity, sizeof(char*));
        if (!hl->items) { free(hl); return NULL; }
        hl->count = 0;
        hl->cursor = -1;
        HashMap_Put(g_history, feature, hl);
    }
    return hl;
}

static HistoryList* get_existing(const char* feature) {
    if (!g_history || !feature) return NULL;
    return (HistoryList*)HashMap_Get(g_history, feature);
}

void History_Init(void) {
    if (g_history) return;
    g_history = HashMap_Create(32);
    FILE* f = fopen(HISTORY_FILE, "r");
    char line[1024];
    char current_feature[256] = {0};
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        if (strncmp(line, "===", 3) == 0) {
            char* p = strstr(line, "FEATURE:");
            if (p) {
                p += 8;
                char* end = strstr(p, "===");
                if (end) *end = '\0';
                strncpy(current_feature, p, sizeof(current_feature)-1);
                current_feature[sizeof(current_feature)-1]='\0';
            }
            continue;
        }
        if (current_feature[0] == '\0') continue;
        History_Add(current_feature, line);
        HistoryList* hl = get_existing(current_feature);
        if (hl) hl->cursor = -1;
    }
    fclose(f);
    if (!g_history) return;
    for (int i = 0; i < g_history->size; i++) {
        HashMapEntry* e = g_history->buckets[i];
        while (e) {
            HistoryList* hl = (HistoryList*)e->value;
            if (hl) hl->cursor = -1;
            e = e->next;
        }
    }
}

void History_Shutdown(void) {
    if (!g_history) return;
    FILE* f = fopen(HISTORY_FILE, "w");
    if (f) {
        for (int i = 0; i < g_history->size; i++) {
            HashMapEntry* e = g_history->buckets[i];
            while (e) {
                HistoryList* hl = (HistoryList*)e->value;
                if (hl && hl->count > 0) {
                    fprintf(f, "===FEATURE:%s===\n", e->key);
                    for (int k = hl->count - 1; k >= 0; k--) {
                        if (hl->items[k]) fprintf(f, "%s\n", hl->items[k]);
                    }
                }
                e = e->next;
            }
        }
        fclose(f);
    }
    HashMap_DestroyWith(g_history, free_history_list);
    g_history = NULL;
}

void History_Add(const char* feature, const char* text) {
    if (!feature || !text) return;
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    if (len == 0) return;
    char* dup = strdup(text);
    if (!dup) return;
    while (len > 0 && (dup[len-1] == ' ' || dup[len-1] == '\t' || dup[len-1] == '\r' || dup[len-1] == '\n')) {
        dup[--len] = '\0';
    }
    if (len == 0) { free(dup); return; }

    HistoryList* hl = get_or_create(feature);
    if (!hl) { free(dup); return; }

    for (int i = 0; i < hl->count; i++) {
        if (strcmp(hl->items[i], dup) == 0) {
            free(hl->items[i]);
            for (int k = i; k + 1 < hl->count; k++) hl->items[k] = hl->items[k+1];
            hl->count--;
            break;
        }
    }

    if (hl->count >= hl->capacity) {
        int new_cap = hl->capacity * 2;
        if (new_cap < 16) new_cap = 16;
        if (new_cap > HISTORY_MAX_PER_FEATURE * 2) new_cap = HISTORY_MAX_PER_FEATURE * 2;
        char** new_items = realloc(hl->items, new_cap * sizeof(char*));
        if (!new_items) { free(dup); return; }
        hl->items = new_items;
        hl->capacity = new_cap;
    }
    for (int i = hl->count; i > 0; i--) hl->items[i] = hl->items[i-1];
    hl->items[0] = dup;
    hl->count++;
    if (hl->count > HISTORY_MAX_PER_FEATURE) {
        free(hl->items[hl->count-1]);
        hl->count = HISTORY_MAX_PER_FEATURE;
    }
    hl->cursor = -1;
}

bool History_CycleUp(const char* feature, char* out, size_t out_cap) {
    if (!feature || !out || out_cap == 0) return false;
    HistoryList* hl = get_existing(feature);
    if (!hl || hl->count == 0) return false;
    if (hl->cursor == -1) hl->cursor = 0;
    else if (hl->cursor < hl->count - 1) hl->cursor++;
    else hl->cursor = 0; 
    const char* src = hl->items[hl->cursor];
    if (!src) return false;
    strncpy(out, src, out_cap-1);
    out[out_cap-1]='\0';
    return true;
}

bool History_CycleDown(const char* feature, char* out, size_t out_cap) {
    if (!feature || !out || out_cap == 0) return false;
    HistoryList* hl = get_existing(feature);
    if (!hl || hl->cursor == -1) return false;
    if (hl->cursor == 0) {
        hl->cursor = -1;
        out[0]='\0';
        return true;
    } else {
        hl->cursor--;
        const char* src = hl->items[hl->cursor];
        if (!src) return false;
        strncpy(out, src, out_cap-1);
        out[out_cap-1]='\0';
        return true;
    }
}

void History_ResetCursor(const char* feature) {
    HistoryList* hl = get_existing(feature);
    if (hl) hl->cursor = -1;
}

int History_Count(const char* feature) {
    HistoryList* hl = get_existing(feature);
    return hl ? hl->count : 0;
}

void History_Clear(const char* feature) {
    if (!g_history || !feature) return;
    HistoryList* hl = (HistoryList*)HashMap_Remove(g_history, feature);
    if (hl) free_history_list(hl);
}
