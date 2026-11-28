#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include "platform/lib.h"

bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n) {
  if (!out || n == 0)
    return false;
  out[0] = '\0';
  if (!addr)
    return false;
  Dl_info info;
  if (!dladdr(addr, &info) || !info.dli_fname)
    return false;
  snprintf(out, n, "%s", info.dli_fname);
  return out[0] != '\0';
}

char *Platform_GetModuleOwnerPathAlloc(void *addr) {
  if (!addr)
    return NULL;
  Dl_info info;
  if (!dladdr(addr, &info) || !info.dli_fname || !info.dli_fname[0])
    return NULL;
  size_t n = strlen(info.dli_fname);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, info.dli_fname, n + 1);
  return out;
}

LibHandle Platform_LoadLibrary(const char *path) {
  if (!path)
    return NULL;
  return dlopen(path, RTLD_NOW);
}

void *Platform_GetSymbol(LibHandle lib, const char *name) {
  if (!lib || !name)
    return NULL;
  return dlsym(lib, name);
}

void *Platform_GetMainSymbol(const char *name) {
  if (!name)
    return NULL;
  return dlsym(RTLD_DEFAULT, name);
}

void Platform_UnloadLibrary(LibHandle lib) {
  if (lib)
    dlclose(lib);
}

const char *Platform_LibExtension(void) { return ".so"; }

const char *Platform_GetLoadError(void) { return dlerror(); }
