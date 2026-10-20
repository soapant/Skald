#include "platform/lib.h"
#include <stddef.h>
#include <stdio.h>
#include <windows.h>

#include "core/timer.h"
LibHandle Platform_LoadLibrary(const char* path) {
        PROFILE_START(_pt);
if (!path) { PROFILE_END_VAR(_pt, "Platform_LoadLibrary"); return NULL; }
    { PROFILE_END_VAR(_pt, "Platform_LoadLibrary"); return (LibHandle)LoadLibraryA(path); }
}
void* Platform_GetSymbol(LibHandle lib, const char* name) {
        PROFILE_START(_pt);
if (!lib || !name) { PROFILE_END_VAR(_pt, "Platform_GetSymbol"); return NULL; }
    { PROFILE_END_VAR(_pt, "Platform_GetSymbol"); return (void*)GetProcAddress((HMODULE)lib, name); }
}
void Platform_UnloadLibrary(LibHandle lib) {
        PROFILE_START(_pt);
if (lib) FreeLibrary((HMODULE)lib);
}
const char* Platform_LibExtension(void) {
    PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_LibExtension"); return ".dll"; } 
}
const char* Platform_GetLoadError(void) {
    PROFILE_START(_pt);
static char buf[512];
  DWORD err = GetLastError();
  if (err == 0) { PROFILE_END_VAR(_pt, "Platform_GetLoadError"); return "unknown error"; }
  DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, buf, sizeof(buf), NULL);
  if (n == 0) {
    snprintf(buf, sizeof(buf), "error %lu", (unsigned long)err);
  }
  { PROFILE_END_VAR(_pt, "Platform_GetLoadError"); return buf; }
}

bool Platform_GetModuleOwnerPath(void *addr, char *out, size_t n) {
        PROFILE_START(_pt);
if (!out || n == 0) { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return false; }
    out[0] = '\0';
    if (!addr) { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return false; }
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
    { PROFILE_END_VAR(_pt, "Platform_GetModuleOwnerPath"); return out[0] != '\0'; }
}
