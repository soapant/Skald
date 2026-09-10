#include "platform/lib.h"
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
LibHandle Platform_LoadLibrary(const char* path) {
    if (!path) return NULL;
    return (LibHandle)LoadLibraryA(path);
}
void* Platform_GetSymbol(LibHandle lib, const char* name) {
    if (!lib || !name) return NULL;
    return (void*)GetProcAddress((HMODULE)lib, name);
}
void Platform_UnloadLibrary(LibHandle lib) {
    if (lib) FreeLibrary((HMODULE)lib);
}
const char* Platform_LibExtension(void) { return ".dll"; }
#else
#include <dlfcn.h>
LibHandle Platform_LoadLibrary(const char* path) {
    if (!path) return NULL;
    return dlopen(path, RTLD_NOW);
}
void* Platform_GetSymbol(LibHandle lib, const char* name) {
    if (!lib || !name) return NULL;
    return dlsym(lib, name);
}
void Platform_UnloadLibrary(LibHandle lib) {
    if (lib) dlclose(lib);
}
const char* Platform_LibExtension(void) {
#ifdef __APPLE__
    return ".dylib";
#else
    return ".so";
#endif
}
#endif