#ifndef MINECRAFT_H
#define MINECRAFT_H

#include "common.h"
void minecraft_type_command(const char* command) {
#if defined(_WIN32)
    INPUT t[2] = {0};
    t[0].type = INPUT_KEYBOARD; 
    t[0].ki.wVk = 'T';
    t[1].type = INPUT_KEYBOARD; 
    t[1].ki.wVk = 'T'; 
    t[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, t, sizeof(INPUT));
    Sleep(60);

    for (int i = 0; command[i]; i++) {
        INPUT ch[2] = {0};
        ch[0].type = INPUT_KEYBOARD; 
        ch[0].ki.wVk = command[i];
        ch[1].type = INPUT_KEYBOARD; 
        ch[1].ki.wVk = command[i]; 
        ch[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, ch, sizeof(INPUT));
    }

    INPUT enter[2] = {0};
    enter[0].type = INPUT_KEYBOARD; 
    enter[0].ki.wVk = VK_RETURN;
    enter[1].type = INPUT_KEYBOARD; 
    enter[1].ki.wVk = VK_RETURN; 
    enter[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, enter, sizeof(INPUT));
#else
    (void)command;
#endif
}

#endif
