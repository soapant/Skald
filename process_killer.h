#ifndef PROCESS_KILLER_H
#define PROCESS_KILLER_H

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdbool.h>
#include <shlwapi.h>

#define MAX_PK_APPS 256

static char G_PK_LIST[MAX_PK_APPS][128];
static int G_PK_COUNT = 0;

// Programmatically filter out core protected Windows OS components to save list space
static bool is_system_noise(const char* exe) {
    const char* noise[] = {
        "[system process]", "system", "idle", "svchost.exe", "lsass.exe", 
        "services.exe", "wininit.exe", "csrss.exe", "smss.exe", 
        "winlogon.exe", "spoolsv.exe", "dasHost.exe", "conhost.exe",
        "sihost.exe", "RuntimeBroker.exe", "ctfmon.exe", "SearchHost.exe",
        "SearchIndexer.exe", "wmiprvse.exe", "dllhost.exe", "audiodg.exe",
        "taskhostw.exe", "dwm.exe", "fontdrvhost.exe", "Registry","TextInputHost.exe"
    };
    for (size_t i = 0; i < sizeof(noise) / sizeof(noise[0]); i++) {
        if (StrCmpIA(exe, noise[i]) == 0) return true;
    }
    return false;
}

// Scans the raw NT kernel process table directly
static void collect_running_processes() {
    G_PK_COUNT = 0;

    // Take a snapshot of all active processes across the entire OS
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            // Filter out system noise and check for array slot limits
            if (!is_system_noise(pe.szExeFile)) {
                
                // Keep it clean: check for duplicate names if multiple instances exist
                bool exists = false;
                for (int i = 0; i < G_PK_COUNT; i++) {
                    if (StrCmpIA(G_PK_LIST[i], pe.szExeFile) == 0) {
                        exists = true;
                        break;
                    }
                }

                if (!exists && G_PK_COUNT < MAX_PK_APPS) {
                    strncpy(G_PK_LIST[G_PK_COUNT], pe.szExeFile, 127);
                    G_PK_LIST[G_PK_COUNT][127] = '\0';
                    G_PK_COUNT++;
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
}

// Terminates every instance matching the string name
static void terminate_process_by_name(const char* exe_name) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (StrCmpIA(pe.szExeFile, exe_name) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    printf("[PROCESS-KILLER] Terminated: %s\n", exe_name);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
}

#endif
