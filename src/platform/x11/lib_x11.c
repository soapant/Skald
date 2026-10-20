#define _GNU_SOURCE
#include "core/timer.h"
#include "platform/lib.h"
#include <stddef.h>
#include <stdio.h>
#include <dlfcn.h>

bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n) {
        PROFILE_START(_pt);
if (!out || n == 0) { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return false; }
    out[0] = '\0';
    if (!addr) { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return false; }
    Dl_info info;
    if (!dladdr(addr, &info) || !info.dli_fname) { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return false; }
    snprintf(out, n, "%s", info.dli_fname);
    { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return out[0] != '\0'; }
}

LibHandle Platform_LoadLibrary(const char* path) {
        PROFILE_START(_pt);
if (!path) { PROFILE_END_VAR(_pt, "Platform_LoadLibrary"); return NULL; }
    { PROFILE_END_VAR(_pt, "Platform_LoadLibrary"); return dlopen(path, RTLD_NOW); }
}
void* Platform_GetSymbol(LibHandle lib, const char* name) {
        PROFILE_START(_pt);
if (!lib || !name) { PROFILE_END_VAR(_pt, "Platform_GetSymbol"); return NULL; }
    { PROFILE_END_VAR(_pt, "Platform_GetSymbol"); return dlsym(lib, name); }
}
void Platform_UnloadLibrary(LibHandle lib) {
        PROFILE_START(_pt);
if (lib) dlclose(lib);
}
const char* Platform_LibExtension(void) {
    PROFILE_START(_pt);
#ifdef __APPLE__
    { PROFILE_END_VAR(_pt, "Platform_LibExtension"); return ".dylib"; }
#else
    { PROFILE_END_VAR(_pt, "Platform_LibExtension"); return ".so"; }
#endif
}
const char* Platform_GetLoadError(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetLoadError"); return dlerror(); }
}
