#include "common.h"
#include "bindings/macro_bindings.h"
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
