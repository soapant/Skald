#ifndef PLATFORM_LIB_H
#define PLATFORM_LIB_H

#include <stddef.h>
#include <stdbool.h>

typedef void* LibHandle;

LibHandle Platform_LoadLibrary(const char* path);
void* Platform_GetSymbol(LibHandle lib, const char* name);
void Platform_UnloadLibrary(LibHandle lib);
const char* Platform_LibExtension(void);
const char* Platform_GetLoadError(void);

/* Resolves the shared library that owns the given code address, so callers can
   tell which feature module a callback came from. Returns false if unknown. */
bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n);

#endif