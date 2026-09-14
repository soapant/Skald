#include "core/platform.h"
#include "deps/log.h"
#include <windows.h>

void Platform_EmitKey(int vk_code, int down) {
    log_warn("Platform_EmitKey win32 vk=%d down=%d", vk_code, down);
    INPUT inp = {0};
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = (WORD)vk_code;
    inp.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (vk_code == VK_UP || vk_code == VK_DOWN || vk_code == VK_LEFT || vk_code == VK_RIGHT || vk_code == VK_RCONTROL || vk_code == VK_RMENU) {
        inp.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    SendInput(1, &inp, sizeof(INPUT));
}

bool Platform_IsRemapActive(void) {
    return false;
}
