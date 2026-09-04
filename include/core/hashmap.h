#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
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

HashMap* HashMap_Create(int initial_size);
void HashMap_Destroy(HashMap* map);
void HashMap_Put(HashMap* map, const char* key, void* value);
void* HashMap_Get(HashMap* map, const char* key);
bool HashMap_Contains(HashMap* map, const char* key);
void* HashMap_Remove(HashMap* map, const char* key);
char** HashMap_GetKeys(HashMap* map, int* count);
int HashMap_Size(HashMap* map);

#endif
