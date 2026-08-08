#ifndef MACRO_BINDINGS_H
#define MACRO_BINDINGS_H

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

extern const MacroBinding G_MACRO_BINDINGS[7];

#define MACRO_BINDING_COUNT (sizeof(G_MACRO_BINDINGS) / sizeof(G_MACRO_BINDINGS[0]))

#endif
