#ifndef NOTIFICATION_GETTER_H
#define NOTIFICATION_GETTER_H

#include <wchar.h>

typedef void (*NtfyCallback)(const char* text);

typedef struct {
    wchar_t path[256];
    NtfyCallback callback;
} NtfyConfig;

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>

DWORD WINAPI NtfyListenerThread(LPVOID lpParam);

void StartNtfyDaemon(const wchar_t* topic, NtfyCallback callback);
#endif

#endif
