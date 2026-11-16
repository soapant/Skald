#ifndef LIB_H
#define LIB_H

#include <stddef.h>
#include <stdbool.h>

typedef void *LibHandle;

LibHandle Platform_LoadLibrary(const char *path);
void *Platform_GetSymbol(LibHandle lib, const char *name);
void Platform_UnloadLibrary(LibHandle lib);
const char *Platform_LibExtension(void);
const char *Platform_GetLoadError(void);
bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n);

#endif
