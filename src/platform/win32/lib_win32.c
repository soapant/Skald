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
