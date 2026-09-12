#include "platform/lib.h"
#include <stddef.h>
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
