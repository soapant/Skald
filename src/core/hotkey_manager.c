#include "helpers.h"
#include <stdlib.h>
#include <string.h>

static HotkeyBinding* g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;

void hotkey_register_internal(HotkeyBinding binding) {
    if (g_hotkey_count >= g_hotkey_capacity) {
        int new_cap = g_hotkey_capacity == 0 ? 16 : g_hotkey_capacity * 2;
        HotkeyBinding* new_arr = realloc(g_hotkeys, new_cap * sizeof(HotkeyBinding));
        if (!new_arr) return;
        g_hotkeys = new_arr;
        g_hotkey_capacity = new_cap;
    }
    g_hotkeys[g_hotkey_count++] = binding;
}

bool hotkey_process(int vk_code, int modifiers, const char* title) {
    for (int i = 0; i < g_hotkey_count; i++) {
        HotkeyBinding* b = &g_hotkeys[i];
        if (b->vk_code == vk_code && b->modifiers == modifiers) {
            if (b->hotkey_callback) b->hotkey_callback();
            return true;
        }
    }
    return false;
}
