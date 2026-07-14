// helpers.h
#ifndef HELPERS_H
#define HELPERS_H

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>  
#include <string.h> 
#include <shlwapi.h>
#include <stdlib.h>
#include "types.h"
#include "config.h"
#include "common_ui.h"

// ============================================================
// HOTKEY SYSTEM - Single entry point for all hotkeys
// ============================================================

// Hotkey handler function type
typedef void (*HotkeyHandler)(void);

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    HotkeyHandler handler;
    const char* name;
} HotkeyBinding;

// helpers.h - ProcessHotkey uses exact modifier matching
static inline bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        // Exact match on both key AND modifiers
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;  // Exact match
        
        // Check blacklist
        if (b->blacklist && b->blacklist_count > 0) {
            bool blacklisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
                if (StrStrIA(current_title, b->blacklist[j]) != NULL) {
                    blacklisted = true;
                    break;
                }
            }
            if (blacklisted) return false;
        }
        
        // Don't trigger if popup is active
        extern HWND hPopupWnd;
        if (IsPopupActive(hPopupWnd)) return false;
        
        // Execute the handler
        if (b->handler) {
            printf("[HOTKEY] %s triggered (mods: 0x%X)\n", b->name, b->modifiers);
            b->handler();
            return true;
        }
    }
    return false;
}
// ============================================================
// ZOXIDE HELPER
// ============================================================

typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

static inline char* QueryZoxidePath(const char* query) {
    char command[512];
    snprintf(command, sizeof(command), "zoxide query %s", query);
    
    static char path[1024];
    path[0] = '\0';
    FILE* pipe = popen(command, "r");
    if (pipe) {
        fgets(path, sizeof(path), pipe);
        pclose(pipe);
        path[strcspn(path, "\r\n")] = '\0';
    }
    return (strlen(path) > 0) ? path : NULL;
}

static inline void TriggerZoxideWithOptions(const ZoxideOptions* options) {
    extern HWND hPopupWnd;
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    const char* prompt[1] = { options->prompt_title };
    char* query = PromptUser(prompt, options->icon, 1, 220, 150, 32, 
                             NULL, NULL, NULL, NULL, NULL);
    
    if (!query || strlen(query) == 0) return;
    
    char* path = QueryZoxidePath(query);
    if (!path) {
        printf("[ZOXIDE] No match found for: %s\n", query);
        return;
    }
    
    if (options->open_neovide) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start neovide \"%s\"", path);
        system(cmd);
        printf("[ZOXIDE] Opened Neovide in: %s\n", path);
    } else if (options->open_yazi) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\" -e yazi", path);
        system(cmd);
        printf("[ZOXIDE] Opened Yazi in: %s\n", path);
    } else if (options->open_explorer) {
        ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
        printf("[ZOXIDE] Opened Explorer in: %s\n", path);
    } else {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\"", path);
        system(cmd);
        printf("[ZOXIDE] Opened Alacritty in: %s\n", path);
    }
}

// ============================================================
// SHELL HELPER
// ============================================================

static inline bool ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = (async ? SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK : SEE_MASK_FLAG_NO_UI);
    sei.lpVerb = verb ? verb : "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExA(&sei);
}

static inline bool LaunchApp(const char* path) {
    return ShellExecuteWithVerb(path, "open", true);
}

static inline bool LaunchAppAsAdmin(const char* path) {
    return ShellExecuteWithVerb(path, "runas", true);
}
static inline bool IsCtrlPressed(void) {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

#endif // HELPERS_H
