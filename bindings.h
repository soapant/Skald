#include "config.h"
#include <windows.h>
#include "app_launcher.h"
#include "macro_bindings.h"
#include "vim_motions.h"
#include "window_switcher.h"
#include "vim_bindings.h"
#include "modal.h"
#include "character_sequence.h"

static inline bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count) {
    extern HWND hPopupWnd;
    extern HWND hEditControl;
    
    if (!g_vim_mode_enabled) return false;
    if (!hPopupWnd || !IsWindow(hPopupWnd)) return false;
    if (!hEditControl || !IsWindow(hEditControl)) return false;
    if (GetFocus() != hEditControl) return false;
    
    // ============================================================
    // CHECK IF SHIFT IS HELD AS A MODIFIER
    // ============================================================
    bool shift_held = (modifiers & HOTKEY_MOD_SHIFT) != 0;
    bool ctrl_held = (modifiers & HOTKEY_MOD_CTRL) != 0;
    bool alt_held = (modifiers & HOTKEY_MOD_ALT) != 0;
    
    if (pressed_vk == VK_ESCAPE) {
        if (!IsNormalMode()) {
            SequenceReset();
            *out_action = MOTION_ENTER_NORMAL;
            *out_sequence = NULL;
            *out_typed_count = 0;
            return true;
        }
        return false;
    }
    
    char key_char = (char)pressed_vk;
    char seq_char = tolower(key_char);
    
    bool in_insert = IsInsertMode();
    bool in_normal = IsNormalMode();
    bool in_visual = IsVisualMode();
    
    if (in_insert && SequenceIsWaiting()) {
        if (SequenceTimedOut()) {
            SequenceReset();
            return false;
        }
        
        SequenceAddKey(seq_char, false);
        SequenceStartTimer();
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceMatches(b->sequence)) {
                printf("[VIM] ✅ INSERT SEQUENCE: '%s'\n", b->sequence);
                *out_action = b->action_id;
                *out_sequence = b->sequence;
                *out_typed_count = SequenceTypedCount();
                SequenceReset();
                return true;
            }
        }
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceStartsWith(b->sequence)) {
                printf("[VIM] INSERT prefix: '%s'\n", b->sequence);
                return true;
            }
        }
        
        printf("[VIM] No match for sequence, letting keys through\n");
        SequenceReset();
        return false;
    }
    
    if (in_insert) {
        if (!shift_held) {
            SequenceAddKey(seq_char, true);
            
            bool is_prefix = false;
            for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
                const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
                if (b->mode == MODE_INSERT && SequenceStartsWith(b->sequence)) {
                    is_prefix = true;
                    break;
                }
            }
            
            if (is_prefix) {
                SequenceStartTimer();
                printf("[VIM] INSERT timer started for: '%s'\n", SequenceGet());
                *out_sequence = NULL;
                *out_typed_count = 0;
                return false;
            }
            
            SequenceReset();
            return false;
        } else {
            return false;
        }
    }
    
    if (in_normal) {
        if (SequenceTimedOut()) {
            SequenceReset();
            return false;
        }
        if (pressed_vk == VK_RETURN || pressed_vk == 160) {
            SequenceReset();
            *out_sequence = NULL;
            *out_typed_count = 0;
            return false;
        }
        
        if (!shift_held) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            
            for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
                const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
                if (b->mode == MODE_NORMAL) {
                    if (SequenceMatches(b->sequence)) {
                        printf("[VIM] ✅ NORMAL SEQUENCE: '%s'\n", b->sequence);
                        *out_action = b->action_id;
                        *out_sequence = b->sequence;
                        *out_typed_count = SequenceTypedCount();
                        SequenceReset();
                        return true;
                    }
                    if (SequenceStartsWith(b->sequence)) {
                        printf("[VIM] NORMAL prefix: '%s'\n", b->sequence);
                        return true;
                    }
                }
            }
            
            SequenceReset();
        }
        
        for (int i = 0; i < VIM_BINDING_COUNT; i++) {
            const MotionBinding* b = &G_VIM_BINDINGS[i];
            if (b->vk_code == pressed_vk && b->modifiers == modifiers && b->mode == MODE_NORMAL) {
                *out_action = b->action_id;
                *out_sequence = NULL;
                *out_typed_count = 0;
                return true;
            }
        }
        
        return true;
    }
    
    if (in_visual) {
        return true;
    }
    
    return false;
}
static inline void RemoveSequenceFromEdit(int typed_count) {
    extern HWND hEditControl;
    
    printf("[VIM-DEBUG] RemoveSequenceFromEdit: typed_count=%d\n", typed_count);
    
    if (!hEditControl || !IsWindow(hEditControl)) {
        printf("[VIM-DEBUG] RemoveSequenceFromEdit: hEditControl invalid!\n");
        return;
    }
    if (typed_count <= 0) {
        printf("[VIM-DEBUG] RemoveSequenceFromEdit: typed_count <= 0, skipping\n");
        return;
    }
    
    char text[512] = {0};
    GetWindowTextA(hEditControl, text, sizeof(text));
    int text_len = (int)strlen(text);
    printf("[VIM-DEBUG] RemoveSequenceFromEdit: text='%s', text_len=%d\n", text, text_len);
    
    if (text_len >= typed_count) {
        text[text_len - typed_count] = '\0';
        SetWindowTextA(hEditControl, text);
        SendMessageA(hEditControl, EM_SETSEL, text_len - typed_count, text_len - typed_count);
        printf("[VIM-DEBUG] Removed %d typed characters, new text='%s'\n", typed_count, text);
    } else {
        printf("[VIM-DEBUG] text_len < typed_count, can't remove\n");
    }
}

static inline void ExecuteMotion(MotionAction action, const char* sequence, int typed_count) {
    printf("[VIM-DEBUG] ExecuteMotion: action=%d, sequence='%s', typed_count=%d\n", action, sequence ? sequence : "NULL", typed_count);
    
    if (typed_count > 0) {
        RemoveSequenceFromEdit(typed_count);
    }
    
    
    for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
        const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            printf("[VIM-DEBUG] ExecuteMotion: calling sequence handler\n");
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
    
    printf("[VIM-DEBUG] ExecuteMotion: no handler found, checking direct actions\n");
    if (action == MOTION_ENTER_NORMAL) {
        HandleEnterNormal();
    } else if (action == MOTION_ENTER_INSERT_OUT) {
        HandleEnterInsertOut();
    } else if (action == MOTION_ENTER_INSERT_IN) {
        HandleEnterInsertIn();
    } else if (action == MOTION_ENTER_VISUAL) {
        HandleEnterVisual();
    }
    printf("[VIM-DEBUG] ExecuteMotion: done\n");
}
bool CheckAppHotkeyBindings(int pressed_vk, int modifiers) {
    HWND hActiveWnd = GetForegroundWindow();
    if (!hActiveWnd) return false;

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    
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
    
    
    const char* ext = PathFindExtensionA(path);
    
    
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
        
        snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && \"%s\"\"", dir_path, path);
        system(cmd);
        printf("[MISC] Running .bat in: %s\n", dir_path);
        return;
    }
    
    
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
    
    
    if (StrCmpIA(ext, ".exe") == 0) {
        sei.lpFile = path;
        if (ShellExecuteExA(&sei)) {
            printf("[MISC] Launched: %s\n", path);
        } else {
            printf("[MISC] Failed to launch: %s (Error: %lu)\n", path, GetLastError());
        }
        return;
    }
    
    
    if (StrCmpIA(ext, ".url") == 0) {
        
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
    
    
    if (StrCmpIA(ext, ".txt") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "notepad \"%s\"", path);
        system(cmd);
        printf("[MISC] Opening text file: %s\n", path);
        return;
    }
    
    
    if (StrCmpIA(ext, ".lnk") == 0) {
        sei.lpFile = path;
        if (ShellExecuteExA(&sei)) {
            printf("[MISC] Launched shortcut: %s\n", path);
        } else {
            printf("[MISC] Failed to launch shortcut: %s (Error: %lu)\n", path, GetLastError());
        }
        return;
    }
    
    
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
