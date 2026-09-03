#ifndef MACROS_H
#define MACROS_H


#include "core/flapi.h"

void create_tag() {
  printf("CREATE TAG");
  
}

void search_factory() {
  printf("SEARCH Factory");
}

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

static inline void macros_init(void) {
    fl_register_hotkey("Alt+Ctrl+V", HandleFactorioMakeTag, NULL, NULL);
    fl_register_hotkey("Alt+Ctrl+B", HandleFactorioSearchFactory, NULL, NULL);
    fl_register_hotkey("Alt+Shift+C",HandleMinecraftCopy, NULL, NULL);
    fl_register_hotkey("Alt+Shift+V",HandleMinecraftPaste, NULL, NULL);
    fl_register_hotkey("Alt+Shift+Z",HandleMinecraftUndo, NULL, NULL);
    fl_register_hotkey("Alt+Shift+Q",HandleMinecraftUp, NULL, NULL);
    fl_register_hotkey("Alt+Shift+R",HandleMinecraftRotate, NULL, NULL);
}

#endif
