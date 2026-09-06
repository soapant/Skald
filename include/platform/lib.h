#ifndef PLATFORM_LIB_H
#define PLATFORM_LIB_H

typedef void* LibHandle;

LibHandle Platform_LoadLibrary(const char* path);
void* Platform_GetSymbol(LibHandle lib, const char* name);
void Platform_UnloadLibrary(LibHandle lib);
const char* Platform_LibExtension(void);

#endif
