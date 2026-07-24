#ifndef MACRO_BINDINGS_H
#define MACRO_BINDINGS_H

#include "minecraft.h"
#include "factorio.h"
#include "types.h"

static inline void HandleFactorioMakeTag(void) { 
    create_tag(); 
}

static inline void HandleFactorioSearchFactory(void) { 
    search_factory(); 
}

static inline void HandleMinecraftCopy(void) { 
    minecraft_type_command("//copy"); 
}

static inline void HandleMinecraftPaste(void) { 
    minecraft_type_command("//paste -a"); 
}

static inline void HandleMinecraftUndo(void) { 
    minecraft_type_command("//undo"); 
}

static inline void HandleMinecraftUp(void) { 
    minecraft_type_command("//up 1"); 
}

static inline void HandleMinecraftRotate(void) { 
    minecraft_type_command("//rotate 90"); 
}
typedef enum {
    MACRO_NONE = 0,
    MACRO_FACTORIO_MAKE_TAG,
    MACRO_MINECRAFT_PASTE,
    MACRO_MINECRAFT_COPY,
    MACRO_MINECRAFT_ROTATE,
    MACRO_MINECRAFT_UP,
    MACRO_MINECRAFT_UNDO,
    MACRO_FACTORIO_SEARCH_FACTORY,
} MacroAction;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    MacroAction action_id;
    int delay;
    void (*handler)(void);
} MacroBinding;

static const MacroBinding G_MACRO_BINDINGS[] = {
    { 'V', HOTKEY_MOD_ALT, "Factorio", MACRO_FACTORIO_MAKE_TAG, 0, HandleFactorioMakeTag },
    { 'B', HOTKEY_MOD_ALT, "Factorio", MACRO_FACTORIO_SEARCH_FACTORY, 0, HandleFactorioSearchFactory },
    { 'C', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_COPY, 0, HandleMinecraftCopy },
    { 'V', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_PASTE, 0, HandleMinecraftPaste },
    { 'Z', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UNDO, 0, HandleMinecraftUndo },
    { 'Q', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UP, 0, HandleMinecraftUp },
    { 'R', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_ROTATE, 0, HandleMinecraftRotate },
};

#define MACRO_BINDING_COUNT (sizeof(G_MACRO_BINDINGS) / sizeof(G_MACRO_BINDINGS[0]))

#endif 

