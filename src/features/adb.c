#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void run_adb_cmd_sync(const char* args) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "adb.exe %s", args);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

bool connect_phone(void) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    char cmd[] = "adb.exe mdns services";

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(hWrite);
    WaitForSingleObject(pi.hProcess, 2000);

    char buffer[4096] = {0};
    DWORD bytesRead = 0;
    ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    // Strictly match the TLS active connection service string
    char* target = strstr(buffer, "_adb-tls-connect._tcp");
    if (!target) return false;

    // Scan forward to locate the IP:PORT string
    int a, b, c, d, port;
    char* ptr = target;
    while (*ptr) {
        if (sscanf(ptr, "%d.%d.%d.%d:%d", &a, &b, &c, &d, &port) == 5) {
            char ip_port[64];
            snprintf(ip_port, sizeof(ip_port), "%d.%d.%d.%d:%d", a, b, c, d, port);

            // Connect directly to the specific active port
            char connect_args[128];
            snprintf(connect_args, sizeof(connect_args), "connect %s", ip_port);
            run_adb_cmd_sync(connect_args);

            // Give the ADB daemon 200ms to finalize handshake
            Sleep(200);
            return true;
        }
        ptr++;
    }
    return false;
}

void tap(int x, int y) {
    char params[128];
    snprintf(params, sizeof(params), "shell input tap %d %d", x, y);
    run_adb_cmd_sync(params);
}

void mute_messenger_call(void) {
    if (connect_phone()) {
        tap(323, 2148);
    }
}

void toggle_messenger_speaker(void) {
    if (connect_phone()) {
        tap(757, 2148);
    }
}
