#include "core/hashmap.h"
#include "core/types.h"

static unsigned int hash(const char* key, int size) {
    unsigned int h = 0;
    while (*key) {
        h = (h * 31) ^ (unsigned char)*key;
        key++;
    }
    return h % size;
}

HashMap* HashMap_Create(int initial_size) {
    HashMap* map = malloc(sizeof(HashMap));
    if (!map) return NULL;

    map->size = initial_size ? initial_size : 32;
    map->count = 0;
    map->buckets = calloc(map->size, sizeof(HashMapEntry*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

void HashMap_Destroy(HashMap* map) {
    HashMap_DestroyWith(map, NULL);
}

void HashMap_DestroyWith(HashMap* map, void (*free_value)(void* value)) {
    HashMap_DestroyWithKeyValue(map, free_value, NULL);
}

void HashMap_DestroyWithKeyValue(HashMap* map, void (*free_value)(void* value), void (*free_key)(void* key)) {
    if (!map) return;

    for (int i = 0; i < map->size; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            if (free_key) free_key(entry->key);
            else free(entry->key);
            if (free_value && entry->value) free_value(entry->value);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

static bool hashmap_resize(HashMap* map, int new_size) {
    if (!map || new_size < 8) return false;
    HashMapEntry** new_buckets = calloc(new_size, sizeof(HashMapEntry*));
    if (!new_buckets) return false;
    for (int i = 0; i < map->size; i++) {
        HashMapEntry* e = map->buckets[i];
        while (e) {
            HashMapEntry* next = e->next;
            unsigned int idx = hash(e->key, new_size);
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }
    free(map->buckets);
    map->buckets = new_buckets;
    map->size = new_size;
    return true;
}

bool HashMap_TryResize(HashMap* map) {
    if (!map) return false;
    if (map->count > (int)(map->size * 0.75)) {
        return hashmap_resize(map, map->size * 2);
    }
    return true;
}

void HashMap_Put(HashMap* map, const char* key, void* value) {
    (void)HashMap_PutReplace(map, key, value);
}

void* HashMap_PutReplace(HashMap* map, const char* key, void* value) {
    if (!map || !key) return NULL;

    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            void* old = entry->value;
            entry->value = value;
            return old;
        }
        entry = entry->next;
    }

    HashMapEntry* new_entry = malloc(sizeof(HashMapEntry));
    if (!new_entry) return NULL;
    new_entry->key = strdup(key);
    if (!new_entry->key) {
        free(new_entry);
        return NULL;
    }
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->count++;
    HashMap_TryResize(map);
    return NULL;
}

void HashMap_PutWithFree(HashMap* map, const char* key, void* value, void (*free_value)(void*)) {
    void* old = HashMap_PutReplace(map, key, value);
    if (old && free_value) free_value(old);
}

void* HashMap_Get(HashMap* map, const char* key) {
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

bool HashMap_Contains(HashMap* map, const char* key) {
    return HashMap_Get(map, key) != NULL;
}

void* HashMap_Remove(HashMap* map, const char* key) {
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

char** HashMap_GetKeys(HashMap* map, int* count) {
    if (!map || !count) return NULL;
    if (map->count == 0) {
        *count = 0;
        return NULL;
    }

    char** keys = malloc(map->count * sizeof(char*));
    if (!keys) {
        *count = 0;
        return NULL;
    }
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

int HashMap_Size(HashMap* map) {
    return map ? map->count : 0;
}
