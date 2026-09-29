#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "platform/lib.h"
#include <stddef.h>
#include <stdio.h>
#include <dlfcn.h>

bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = '\0';
    if (!addr) return false;
    Dl_info info;
    if (!dladdr(addr, &info) || !info.dli_fname) return false;
    snprintf(out, n, "%s", info.dli_fname);
    return out[0] != '\0';
}

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
const char* Platform_GetLoadError(void) {
    return dlerror();
}
