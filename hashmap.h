#ifndef HASHMAP_H
#define HASHMAP_H

#include "types.h"
#include <stdlib.h>
#include <string.h>

typedef struct HashMapEntry {
    char* key;
    void* value;
    struct HashMapEntry* next;
} HashMapEntry;

typedef struct {
    HashMapEntry** buckets;
    int size;
    int count;
} HashMap;

static inline unsigned int hash(const char* key, int size) {
    unsigned int h = 0;
    while (*key) {
        h = (h * 31) ^ (unsigned char)*key;
        key++;
    }
    return h % size;
}

static inline HashMap* HashMap_Create(int initial_size) {
    HashMap* map = malloc(sizeof(HashMap));
    if (!map) return NULL;
    
    map->size = initial_size ? initial_size : 32;
    map->count = 0;
    map->buckets = calloc(map->size, sizeof(HashMapEntry*));
    
    return map;
}

static inline void HashMap_Destroy(HashMap* map) {
    if (!map) return;
    
    for (int i = 0; i < map->size; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

static inline void HashMap_Put(HashMap* map, const char* key, void* value) {
    if (!map || !key) return;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    HashMapEntry* new_entry = malloc(sizeof(HashMapEntry));
    new_entry->key = _strdup(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->count++;
}

static inline void* HashMap_Get(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

static inline bool HashMap_Contains(HashMap* map, const char* key) {
    return HashMap_Get(map, key) != NULL;
}

static inline void* HashMap_Remove(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            
            void* value = entry->value;
            free(entry->key);
            free(entry);
            map->count--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return NULL;
}

static inline char** HashMap_GetKeys(HashMap* map, int* count) {
    if (!map) return NULL;
    
    char** keys = malloc(map->count * sizeof(char*));
    int idx = 0;
    
    for (int i = 0; i < map->size; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            keys[idx++] = entry->key;
            entry = entry->next;
        }
    }
    
    *count = map->count;
    return keys;
}

static inline int HashMap_Size(HashMap* map) {
    return map ? map->count : 0;
}

static inline char** StringList_Append(char** list, int* count, const char* str) {
    if (!str) return list;
    
    list = realloc(list, (*count + 2) * sizeof(char*));
    if (!list) return NULL;
    
    list[*count] = _strdup(str);
    (*count)++;
    list[*count] = NULL;  
    
    return list;
}

static inline void StringList_Free(char** list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

#endif