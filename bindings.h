#include "config.h"
#include <windows.h>
#include "app_launcher.h"
#include "macro_bindings.h"
#include "window_switcher.h"

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
