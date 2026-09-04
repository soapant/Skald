#ifndef NOTIFICATION_GETTER_H
#define NOTIFICATION_GETTER_H

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

const wchar_t* HttpUrlForNotis = L"doingthistomakeitrarebitch";

typedef void (*NtfyCallback)(const char*);
typedef struct {
    wchar_t path[256];
    NtfyCallback callback;
} NtfyConfig;

DWORD WINAPI NtfyListenerThread(LPVOID lpParam) {
    NtfyConfig* config = (NtfyConfig*)lpParam;
    HINTERNET hSession = WinHttpOpen(L"MinimalNtfy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return 1;
    HINTERNET hConnect = WinHttpConnect(hSession, L"ntfy.sh", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return 1; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", config->path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return 1; }
    WinHttpSetTimeouts(hRequest, 0, 0, 0, 0);
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            while (TRUE) {
                DWORD dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) {
                    break;
                }
                char* buffer = (char*)malloc(dwSize + 1);
                if (!buffer) break;
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded) && dwDownloaded > 0) {
                    buffer[dwDownloaded] = '\0';
                    if (!(dwDownloaded == 1 && buffer[0] == '\n')) {
                        config->callback(buffer);
                    }
                }
                free(buffer);
            }
        }
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    free(config);
    return 0;
}

void StartNtfyDaemon(const wchar_t* topic, NtfyCallback callback) {
    NtfyConfig* config = (NtfyConfig*)malloc(sizeof(NtfyConfig));
    swprintf(config->path, 256, L"/%ls/raw", topic);
    config->callback = callback;
    CreateThread(NULL, 0, NtfyListenerThread, config, 0, NULL);
}
#else
// Not Implemented
typedef void (*NtfyCallback)(const char*);
static inline void StartNtfyDaemon(const wchar_t* topic, NtfyCallback callback) { (void)topic; (void)callback; }
#endif
#endif
