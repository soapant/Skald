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

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+Ctrl+V", HandleFactorioMakeTag,WHITELIST,"Factorio");
    fl_register_hotkey("Alt+Ctrl+B", HandleFactorioSearchFactory,WHITELIST,"Factorio");
    fl_register_hotkey("Alt+Shift+C",HandleMinecraftCopy, WHITELIST,"Minecraft");
    fl_register_hotkey("Alt+Shift+V",HandleMinecraftPaste, WHITELIST,"Minecraft");
    fl_register_hotkey("Alt+Shift+Z",HandleMinecraftUndo, WHITELIST,"Minecraft");
    fl_register_hotkey("Alt+Shift+Q",HandleMinecraftUp, WHITELIST,"Minecraft");
    fl_register_hotkey("Alt+Shift+R",HandleMinecraftRotate,WHITELIST,"Minecraft");
}
