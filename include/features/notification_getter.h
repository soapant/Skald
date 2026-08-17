#include <windows.h>
#include <winhttp.h>

typedef void (*NtfyCallback)(const char* text);

typedef struct {
    wchar_t path[256];
    NtfyCallback callback;
} NtfyConfig;

DWORD WINAPI NtfyListenerThread(LPVOID lpParam);

void StartNtfyDaemon(const wchar_t* topic, NtfyCallback callback);
