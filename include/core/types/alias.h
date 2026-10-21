#ifndef TYPES_ALIAS_H
#define TYPES_ALIAS_H

/* Kept dependency-free on purpose: config/config.c is hot-recompiled on every
   config change, and pulling in core/types.h costs ~1s of <windows.h> parsing
   on MinGW. Nothing here may include a platform header. */

typedef struct {
   char* alias;
   char* text;
   char* feature;
} AliasEntry;

#endif
