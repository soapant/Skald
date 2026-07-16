#include "config.h"
#include <windows.h>
#include "app_launcher.h"
#include "macro_bindings.h"
#include "window_switcher.h"
#include "vim_bindings.h"
#include "modal.h"
#include "character_sequence.h"

static inline bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action) {
    extern HWND hPopupWnd;
    extern HWND hEditControl;
    
    printf("[VIM-DEBUG] === CheckVimMotion START ===\n");
    printf("[VIM-DEBUG] Key: 0x%X (%c), Modifiers: 0x%X, Mode: %s\n", 
           pressed_vk, pressed_vk, modifiers, GetModeName());
    
    if (!g_vim_mode_enabled) {
        printf("[VIM-DEBUG] Vim mode disabled\n");
        return false;
    }
    if (!hPopupWnd || !IsWindow(hPopupWnd)) {
        printf("[VIM-DEBUG] Popup not active\n");
        return false;
    }
    if (!hEditControl || !IsWindow(hEditControl)) {
        printf("[VIM-DEBUG] Edit control not active\n");
        return false;
    }
    if (GetFocus() != hEditControl) {
        printf("[VIM-DEBUG] Edit doesn't have focus\n");
        return false;
    }
    
    if (pressed_vk == VK_ESCAPE) {
        printf("[VIM-DEBUG] Escape pressed\n");
        if (!IsNormalMode()) {
            printf("[VIM-DEBUG] Escape -> Going to NORMAL mode\n");
            SequenceReset();
            *out_action = MOTION_ENTER_NORMAL;
            return true;
        }
        printf("[VIM-DEBUG] Already in NORMAL, escape ignored\n");
        return false;
    }
    
    if (pressed_vk == VK_CONTROL || pressed_vk == VK_SHIFT || pressed_vk == VK_MENU) {
        printf("[VIM-DEBUG] Modifier key, ignoring\n");
        return false;
    }
    
    // ============================================================
    // Convert to lowercase for case-insensitive matching
    // ============================================================
    char key_char = (char)pressed_vk;
    char seq_char = tolower(key_char);
    printf("[VIM-DEBUG] Key: '%c' -> lowercase: '%c'\n", key_char, seq_char);
    
    bool in_insert = IsInsertMode();
    bool in_normal = IsNormalMode();
    bool in_visual = IsVisualMode();
    
    printf("[VIM-DEBUG] Modes: INSERT=%d, NORMAL=%d, VISUAL=%d\n", in_insert, in_normal, in_visual);
    
    // ============================================================
    // INSERT MODE - Only check INSERT sequences
    // ============================================================
    if (in_insert) {
        printf("[VIM-DEBUG] In INSERT mode - checking INSERT sequences\n");
        SequenceAddKey(seq_char);
        SequenceStartTimer();
        printf("[VIM-DEBUG] Current sequence: '%s' (len=%d)\n", SequenceGet(), SequenceLen());
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT) {
                if (SequenceMatches(b->sequence)) {
                    printf("[VIM-DEBUG] ✅ INSERT SEQUENCE MATCH: '%s'\n", b->sequence);
                    *out_action = b->action_id;
                    SequenceReset();
                    return true;
                }
                if (SequenceStartsWith(b->sequence)) {
                    printf("[VIM-DEBUG] INSERT prefix: '%s' starts with '%s'\n", b->sequence, SequenceGet());
                    return true;  // Wait for more keys
                }
            }
        }
        
        printf("[VIM-DEBUG] No INSERT sequence match, resetting and passing through\n");
        SequenceReset();
        return false;  // Pass key through in INSERT mode
    }
    
    // ============================================================
    // NORMAL MODE - Check NORMAL sequences
    // ============================================================
    if (in_normal) {
        printf("[VIM-DEBUG] In NORMAL mode - checking NORMAL sequences\n");
        SequenceAddKey(seq_char);
        SequenceStartTimer();
        printf("[VIM-DEBUG] Current sequence: '%s' (len=%d)\n", SequenceGet(), SequenceLen());
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_NORMAL) {
                if (SequenceMatches(b->sequence)) {
                    printf("[VIM-DEBUG] ✅ NORMAL SEQUENCE MATCH: '%s'\n", b->sequence);
                    *out_action = b->action_id;
                    SequenceReset();
                    return true;
                }
                if (SequenceStartsWith(b->sequence)) {
                    printf("[VIM-DEBUG] NORMAL prefix: '%s' starts with '%s'\n", b->sequence, SequenceGet());
                    return true;  // Wait for more keys
                }
            }
        }
        
        printf("[VIM-DEBUG] No NORMAL sequence match, resetting\n");
        SequenceReset();
        
        // Check single character bindings
        printf("[VIM-DEBUG] Checking single character bindings in NORMAL mode\n");
        for (int i = 0; i < VIM_BINDING_COUNT; i++) {
            const MotionBinding* b = &G_VIM_BINDINGS[i];
            if (b->vk_code == pressed_vk && b->modifiers == modifiers && b->mode == MODE_NORMAL) {
                printf("[VIM-DEBUG] ✅ SINGLE BINDING: '%c' -> action %d\n", pressed_vk, b->action_id);
                *out_action = b->action_id;
                return true;
            }
        }
        
        printf("[VIM-DEBUG] No binding found in NORMAL mode - swallowing\n");
        return true;  // Swallow in NORMAL mode
    }
    
    // ============================================================
    // VISUAL MODE
    // ============================================================
    if (in_visual) {
        printf("[VIM-DEBUG] In VISUAL mode\n");
        // Add visual mode handling here later
        return true;  // Swallow for now
    }
    
    printf("[VIM-DEBUG] === CheckVimMotion END ===\n\n");
    return false;
}
static inline void ExecuteMotion(MotionAction action) {
    for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
        const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler();
            return;
        }
    }
    
    for (int i = 0; i < VIM_BINDING_COUNT; i++) {
        const MotionBinding* b = &G_VIM_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler();
            return;
        }
    }
    
    if (action == MOTION_ENTER_NORMAL) {
        HandleEnterNormal();
    } else if (action == MOTION_ENTER_INSERT) {
        HandleEnterInsert();
    } else if (action == MOTION_ENTER_VISUAL) {
        HandleEnterVisual();
    }
}

bool CheckAppHotkeyBindings(int pressed_vk, int modifiers) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    // Initialize the cache if it's empty
    if (G_APP_COUNT == 0) {
        initialize_app_cache();
    }
    
    for (int i = 0; i < APP_HOTKEY_COUNT; i++) {
        if (G_APP_HOTKEYS[i].vk_code == pressed_vk && 
            G_APP_HOTKEYS[i].modifiers == modifiers) {
            
            if (strcmp(G_APP_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_APP_HOTKEYS[i].exe_name) != NULL) {
                
                printf("[APP-HOTKEY] Launching: %s\n", G_APP_HOTKEYS[i].app_name);
                launch_app_by_name(G_APP_HOTKEYS[i].app_name);
                return true;
            }
        }
    }
    return false;
}
void RunMiscApplication(const char* path) {
    if (path == NULL || strlen(path) == 0) return;
    
    // Get the file extension
    const char* ext = PathFindExtensionA(path);
    
    // Default: open with ShellExecute
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
    sei.lpVerb = "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    
    if (StrCmpIA(ext, ".bat") == 0) {
        char dir_path[MAX_PATH];
        strncpy(dir_path, path, sizeof(dir_path) - 1);
        PathRemoveFileSpecA(dir_path);
        
        char cmd[2048];
        // Use /c instead of /k - terminal closes after the bat finishes
        snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && \"%s\"\"", dir_path, path);
        system(cmd);
        printf("[MISC] Running .bat in: %s\n", dir_path);
        return;
    }
    
    // Handle .png, .jpg, .jpeg, .bmp - open with mspaint
    if (StrCmpIA(ext, ".png") == 0 || 
        StrCmpIA(ext, ".jpg") == 0 || 
        StrCmpIA(ext, ".jpeg") == 0 || 
        StrCmpIA(ext, ".bmp") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", path);
        system(cmd);
        printf("[MISC] Opening image with mspaint: %s\n", path);
        return;
    }
    
    // Handle .exe - launch directly
    if (StrCmpIA(ext, ".exe") == 0) {
        sei.lpFile = path;
        if (ShellExecuteExA(&sei)) {
            printf("[MISC] Launched: %s\n", path);
        } else {
            printf("[MISC] Failed to launch: %s (Error: %lu)\n", path, GetLastError());
        }
        return;
    }
    
    // Handle .url - open in browser
    if (StrCmpIA(ext, ".url") == 0) {
        // Read the URL from the .url file
        char url[2048] = {0};
        FILE* f = fopen(path, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "URL=", 4) == 0) {
                    strncpy(url, line + 4, sizeof(url) - 1);
                    url[strcspn(url, "\r\n")] = '\0';
                    break;
                }
            }
            fclose(f);
        }
        if (strlen(url) > 0) {
            ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
            printf("[MISC] Opening URL: %s\n", url);
        } else {
            printf("[MISC] Failed to read URL from: %s\n", path);
        }
        return;
    }
    
    // Handle .txt - open with notepad
    if (StrCmpIA(ext, ".txt") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "notepad \"%s\"", path);
        system(cmd);
        printf("[MISC] Opening text file: %s\n", path);
        return;
    }
    
    // Handle .lnk - ShellExecute handles shortcuts
    if (StrCmpIA(ext, ".lnk") == 0) {
        sei.lpFile = path;
        if (ShellExecuteExA(&sei)) {
            printf("[MISC] Launched shortcut: %s\n", path);
        } else {
            printf("[MISC] Failed to launch shortcut: %s (Error: %lu)\n", path, GetLastError());
        }
        return;
    }
    
    // Default: try ShellExecute
    if (ShellExecuteExA(&sei)) {
        printf("[MISC] Opened: %s\n", path);
    } else {
        printf("[MISC] Failed to open: %s (Error: %lu)\n", path, GetLastError());
    }
}

bool CheckMiscHotkeyBindings(int pressed_vk, int modifiers) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < MISC_HOTKEY_COUNT; i++) {
        if (G_MISC_HOTKEYS[i].vk_code == pressed_vk && 
            G_MISC_HOTKEYS[i].modifiers == modifiers) {
            
            if (strcmp(G_MISC_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_MISC_HOTKEYS[i].exe_name) != NULL) {
              
                RunMiscApplication(G_MISC_HOTKEYS[i].misc_path);
              
                return true;
            }
        }
    }
    return false;
}

void open_browser(const char *url) {
    char command[256];

    snprintf(command, sizeof(command), "start %s", url);

    int result = system(command);
    
    if (result == 0) {
        printf("Browser opened successfully.\n");
    } else {
        printf("Failed to open the browser.\n");
    }
}

bool CheckQuickLinkHotkeyBindings(int pressed_vk, int modifiers) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < QUICKLINK_HOTKEY_COUNT; i++) {
        if (G_QUICKLINK_HOTKEYS[i].vk_code == pressed_vk && 
            G_QUICKLINK_HOTKEYS[i].modifiers == modifiers) {
            
            if (strcmp(G_QUICKLINK_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_QUICKLINK_HOTKEYS[i].exe_name) != NULL) {
              
                open_browser(G_QUICKLINK_HOTKEYS[i].QuickLinkQuery);
              
                return true;
            }
        }
    }
    return false;
}

bool CheckMacroBindings(int pressed_vk, int modifiers, MacroAction* out_action, int* out_delay) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (size_t i = 0; i < MACRO_BINDING_COUNT; i++) {
        if (G_MACRO_BINDINGS[i].vk_code == pressed_vk) {
            if (G_MACRO_BINDINGS[i].modifiers != modifiers) {
                continue;
            }
            
            if (strcmp(G_MACRO_BINDINGS[i].exe_name, "*") == 0) {
                *out_action = G_MACRO_BINDINGS[i].action_id;
                *out_delay = G_MACRO_BINDINGS[i].delay;
                return true;
            }
            
            if (StrStrIA(activeExe, G_MACRO_BINDINGS[i].exe_name) != NULL) {
                *out_action = G_MACRO_BINDINGS[i].action_id;
                *out_delay = G_MACRO_BINDINGS[i].delay;
                return true;
            }
        }
    }
    return false;
}

bool CheckTextSnippetBindings(int pressed_vk, int modifiers, const char** out_text) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < TEXT_SNIPPET_COUNT; i++) {
        if (G_TEXT_SNIPPETS[i].vk_code == pressed_vk && 
            G_TEXT_SNIPPETS[i].modifiers == modifiers) {
            
            if (strcmp(G_TEXT_SNIPPETS[i].exe_name, "*") == 0) {
                *out_text = G_TEXT_SNIPPETS[i].text;
                return true;
            }
            
            if (StrStrIA(activeExe, G_TEXT_SNIPPETS[i].exe_name) != NULL) {
                *out_text = G_TEXT_SNIPPETS[i].text;
                return true;
            }
        }
    }
    return false;
}
