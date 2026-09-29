#include "platform/lib.h"
#include <stddef.h>
#include <stdio.h>
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
const char* Platform_LibExtension(void) {
  return ".dll"; 
}
const char* Platform_GetLoadError(void) {
  static char buf[512];
  DWORD err = GetLastError();
  if (err == 0) return "unknown error";
  DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, buf, sizeof(buf), NULL);
  if (n == 0) {
    snprintf(buf, sizeof(buf), "error %lu", (unsigned long)err);
  }
  return buf;
}

bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = '\0';
    if (!addr) return false;
    HMODULE mod = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)addr, &mod)) {
        return false;
    }
    DWORD got = GetModuleFileNameA(mod, out, (DWORD)n);
    if (got == 0) {
        out[0] = '\0';
        return false;
    }
    out[n - 1] = '\0';
    return out[0] != '\0';
}
