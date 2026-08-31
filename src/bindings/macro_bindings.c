#include "common.h"
#include "helpers.h"
#include "bindings/macro_bindings.h"
#include "features/window_switcher.h"
#include "features/minecraft.h"
#include "features/factorio.h"

static void HandleFactorioMakeTag(void) { 
    create_tag(); 
}

static void HandleFactorioSearchFactory(void) { 
    search_factory(); 
}

static void HandleMinecraftCopy(void) { 
    minecraft_type_command("//copy"); 
}

static void HandleMinecraftPaste(void) { 
    minecraft_type_command("//paste -a"); 
}

static void HandleMinecraftUndo(void) { 
    minecraft_type_command("//undo"); 
}

static void HandleMinecraftUp(void) { 
    minecraft_type_command("//up 1"); 
}

static void HandleMinecraftRotate(void) { 
    minecraft_type_command("//rotate 90"); 
}

const MacroBinding G_MACRO_BINDINGS[] = {
    { 'V', HOTKEY_MOD_ALT_CTRL, "Factorio", MACRO_FACTORIO_MAKE_TAG, 0, HandleFactorioMakeTag },
    { 'B', HOTKEY_MOD_ALT_CTRL, "Factorio", MACRO_FACTORIO_SEARCH_FACTORY, 0, HandleFactorioSearchFactory },
    { 'C', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_COPY, 0, HandleMinecraftCopy },
    { 'V', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_PASTE, 0, HandleMinecraftPaste },
    { 'Z', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UNDO, 0, HandleMinecraftUndo },
    { 'Q', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UP, 0, HandleMinecraftUp },
    { 'R', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_ROTATE, 0, HandleMinecraftRotate },
};

bool CheckMacroBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    for (size_t i = 0; i < MACRO_BINDING_COUNT; i++) {
        if (G_MACRO_BINDINGS[i].vk_code == pressed_vk && G_MACRO_BINDINGS[i].modifiers == modifiers) {
            if (fl_exe_matches(G_MACRO_BINDINGS[i].exe_name, activeExe)) {
                if (G_MACRO_BINDINGS[i].handler) G_MACRO_BINDINGS[i].handler();
                return true;
            }
        }
    }
    return false;
}
