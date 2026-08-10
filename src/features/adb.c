#include <stdio.h>
#include <windows.h>

#define PHONE_TARGET "192.168.1.3:33969"

void connect_phone() {
    char params[128];
    snprintf(params, sizeof(params), "connect %s", PHONE_TARGET);
    ShellExecuteA(NULL, "open", "adb.exe", params, NULL, SW_HIDE);
}

void tap(int x, int y) {
    char params[128];
    snprintf(params, sizeof(params), "shell input tap %d %d", x, y);

    ShellExecuteA(NULL, "open", "adb.exe", params, NULL, SW_HIDE);
}

void mute_messenger_call() {
    connect_phone();
    tap(323, 2148);
    return;
}
void toggle_messenger_speaker() {
    connect_phone();
    tap(757, 2148);
    return;
}

