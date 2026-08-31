#include "common.h"
#include "bindings/bindings.h"
#include "bindings/vim_bindings.h"
#include "bindings/macro_bindings.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/window_manager.h"
#include "helpers.h"
#include "features/window_switcher.h"
#include "features/app_launcher.h"
#include "features/focus_switcher.h"
#include "features/terminal_command.h"
#include "core/platform.h"
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#endif
#include <ctype.h>

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count) {
    if (!EnableVimMode) return false;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return false;
    
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return false;
    
    bool in_popup = IsWindowValid(g_hEditPopup);
    
#if defined(_WIN32)
    bool ctrl_held = (GetKeyState(VK_CONTROL) & 0x8000);
    bool shift_held = (GetKeyState(VK_SHIFT) & 0x8000);
#else
    bool ctrl_held = (modifiers & HOTKEY_MOD_CTRL) != 0;
    bool shift_held = (modifiers & HOTKEY_MOD_SHIFT) != 0;
#endif
    
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
            if (b->mode == MODE_INSERT && SequenceMatches(b->sequence) &&
                ScopeMatches(b->scope, in_popup)) {
                *out_action = b->action_id;
                *out_sequence = b->sequence;
                *out_typed_count = SequenceTypedCount();
                SequenceReset();
                return true;
            }
        }
        
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && SequenceStartsWith(b->sequence) &&
                ScopeMatches(b->scope, in_popup)) {
                return true;
            }
        }
        
        SequenceReset();
        return false;
    }
    
    if (in_insert) {
        if (shift_held) {
            return false;
        }
        
        bool is_prefix = false;
        for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
            const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
            if (b->mode == MODE_INSERT && tolower(b->sequence[0]) == seq_char &&
                ScopeMatches(b->scope, in_popup)) {
                is_prefix = true;
                break;
            }
        }
        
        if (is_prefix) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            *out_sequence = NULL;
            *out_typed_count = SequenceTypedCount();
            return false;
        }
        
        SequenceReset();
        return false;
    }
    
    if (in_normal) {
        if (SequenceTimedOut()) {
            SequenceReset();
        }
        
        if (pressed_vk == VK_RETURN || pressed_vk == 160) {
            SequenceReset();
            *out_sequence = NULL;
            *out_typed_count = 0;
            return false;
        }
        if (!shift_held && !ctrl_held) {
            SequenceAddKey(seq_char, true);
            SequenceStartTimer();
            
            for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
                const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
                if (b->mode == MODE_NORMAL && ScopeMatches(b->scope, in_popup)) {
                    if (SequenceMatches(b->sequence)) {
                        *out_action = b->action_id;
                        *out_sequence = b->sequence;
                        *out_typed_count = SequenceTypedCount();
                        SequenceReset();
                        return true;
                    }
                    if (SequenceStartsWith(b->sequence)) {
                        return true;
                    }
                }
            }
            SequenceReset();
        } else {
        }
        
        for (int i = 0; i < VIM_BINDING_COUNT; i++) {
            const MotionBinding* b = &G_VIM_BINDINGS[i];
            if (b->vk_code == pressed_vk && b->modifiers == modifiers && b->mode == MODE_NORMAL &&
                ScopeMatches(b->scope, in_popup)) {
                *out_action = b->action_id;
                *out_sequence = NULL;
                *out_typed_count = 0;
                return true;
            }
        }
        
        return false;
    }
    
    if (in_visual) {
        return false;
    }
    
    return false;
}

void ExecuteMotion(MotionAction action, const char* sequence, int typed_count) {
    VimContext ctx = Vim_FromGlobals();
    if (typed_count > 0 && sequence != NULL && strlen(sequence) > 0 && IsInsertMode()) {
        DeleteLastTypedChar(&ctx);
        if (ctx.state && g_refresh_callback) g_refresh_callback(ctx.state->filter, ctx.hListBox, ctx.lb);
    }
    for (int i = 0; i < VIM_SEQUENCE_BINDING_COUNT; i++) {
        const SequenceBinding* b = &G_VIM_SEQUENCE_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(&ctx);
            return;
        }
    }
    
    for (int i = 0; i < VIM_BINDING_COUNT; i++) {
        const MotionBinding* b = &G_VIM_BINDINGS[i];
        if (b->action_id == action && b->handler != NULL) {
            b->handler(&ctx);
            return;
        }
    }
    
    if (action == MOTION_ENTER_NORMAL) {
        HandleEnterNormal(&ctx);
    } else if (action == MOTION_ENTER_INSERT_OUT) {
        HandleEnterInsertOut(&ctx);
    } else if (action == MOTION_ENTER_INSERT_IN) {
        HandleEnterInsertIn(&ctx);
    } else if (action == MOTION_ENTER_VISUAL) {
        HandleEnterVisual(&ctx);
    }
}
bool CheckAppHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < APP_HOTKEY_COUNT; i++) {
        if (G_APP_HOTKEYS[i].vk_code == pressed_vk && 
            G_APP_HOTKEYS[i].modifiers == modifiers) {
            
#if defined(_WIN32)
            if (strcmp(G_APP_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_APP_HOTKEYS[i].exe_name) != NULL) {
#else
            if (strcmp(G_APP_HOTKEYS[i].exe_name, "*") == 0 ||
                strstr(activeExe, G_APP_HOTKEYS[i].exe_name) != NULL) {
#endif
                
                launch_app_by_name(G_APP_HOTKEYS[i].app_name);
                WindowHandle hwnd = FindWindowByPartialTitle(G_APP_HOTKEYS[i].app_name);
                (void)hwnd;
                return true;
            }
        }
    }
    return false;
}

bool CheckTerminalCommandHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);


    for (int i = 0; i < TERMINAL_COMMAND_HOTKEY_COUNT; i++) {
        if (G_TERMINAL_COMMAND_HOTKEYS[i].vk_code == pressed_vk && 
            G_TERMINAL_COMMAND_HOTKEYS[i].modifiers == modifiers) {
                RunTerminalCommand(G_TERMINAL_COMMAND_HOTKEYS[i].terminal_command, false,G_TERMINAL_COMMAND_HOTKEYS[i].open_in_terminal);
                return true;
            }
        }
    return false;
}

void RunMiscApplication(const char* path) {
    char expanded_path[MAX_PATH];
    if (path != NULL && !Platform_ExpandPath(path, expanded_path, sizeof(expanded_path))) return;
    if (path != NULL) path = expanded_path;
#if defined(_WIN32)
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
#else
    (void)path;
#endif
}

bool CheckMiscHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < MISC_HOTKEY_COUNT; i++) {
        if (G_MISC_HOTKEYS[i].vk_code == pressed_vk && 
            G_MISC_HOTKEYS[i].modifiers == modifiers) {
            
#if defined(_WIN32)
            if (strcmp(G_MISC_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_MISC_HOTKEYS[i].exe_name) != NULL) {
#else
            if (strcmp(G_MISC_HOTKEYS[i].exe_name, "*") == 0 ||
                strstr(activeExe, G_MISC_HOTKEYS[i].exe_name) != NULL) {
#endif
              
                RunMiscApplication(G_MISC_HOTKEYS[i].misc_path);

              
                return true;
            }
        }
    }
    return false;
}

void open_browser(const char *url) {
    if (!url) return;

    char command[1024];

#if defined(_WIN32)
    snprintf(command, sizeof(command), "start %s", url);
#else
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";

    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) {
        snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    } else {
        snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    }

    if (Platform_IsRunningAsRoot()) {
        snprintf(command, sizeof(command), 
                 "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", 
                 real_user, display, xauthority, runtime_dir, url);
    } else {
        snprintf(command, sizeof(command), 
                 "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", 
                 display, xauthority, runtime_dir, url);
    }
#endif

    int result = system(command);
    
    if (result == 0) {
        printf("Browser opened successfully.\n");
    } else {
        printf("Failed to open the browser.\n");
    }
}

bool CheckQuickLinkHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif

    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (int i = 0; i < QUICKLINK_HOTKEY_COUNT; i++) {
        if (G_QUICKLINK_HOTKEYS[i].vk_code == pressed_vk && 
            G_QUICKLINK_HOTKEYS[i].modifiers == modifiers) {
#if defined(_WIN32)
            if (strcmp(G_QUICKLINK_HOTKEYS[i].exe_name, "*") == 0 ||
                StrStrIA(activeExe, G_QUICKLINK_HOTKEYS[i].exe_name) != NULL) {
#else
            if (strcmp(G_QUICKLINK_HOTKEYS[i].exe_name, "*") == 0 ||
                strstr(activeExe, G_QUICKLINK_HOTKEYS[i].exe_name) != NULL) {
#endif
                open_browser(G_QUICKLINK_HOTKEYS[i].QuickLinkQuery);
                if (MessageAfterEachHotkey) {
                  char buffer[64];
                  snprintf(buffer,sizeof(buffer),"[QuickLink] Hotkey Name %s Triggered \n",G_QUICKLINK_HOTKEYS[i].QuickLinkQuery);
                  MSG_BOTTOM(buffer,140,TimeOutForMessages,COLOR_TEXT);
                }
              
                return true;
            }
        }
    }
    return false;
}

bool CheckMacroBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    
    for (size_t i = 0; i < MACRO_BINDING_COUNT; i++) {
        if (G_MACRO_BINDINGS[i].vk_code == pressed_vk) {
            if (G_MACRO_BINDINGS[i].modifiers != modifiers) {
                continue;
            }
#if defined(_WIN32)
            if (strcmp(G_MACRO_BINDINGS[i].exe_name, "*") == 0 || StrStrIA(activeExe, G_MACRO_BINDINGS[i].exe_name) != NULL) {
#else
            if (strcmp(G_MACRO_BINDINGS[i].exe_name, "*") == 0 || strstr(activeExe, G_MACRO_BINDINGS[i].exe_name) != NULL) {
#endif
                G_MACRO_BINDINGS[i].handler();  
            }
        }
    }
    return false;
}
        

bool CheckTextSnippetBindings(int pressed_vk, int modifiers) {

#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};

    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
        
        for (int i = 0; i < TEXT_SNIPPET_COUNT; i++) {
            if (G_TEXT_SNIPPETS[i].vk_code == pressed_vk && 
                G_TEXT_SNIPPETS[i].modifiers == modifiers) {

            
#if defined(_WIN32)
            if (strcmp(G_TEXT_SNIPPETS[i].exe_name, "*") == 0 || StrStrIA(activeExe, G_TEXT_SNIPPETS[i].exe_name) != NULL) {
#else
            if (strcmp(G_TEXT_SNIPPETS[i].exe_name, "*") == 0 || strstr(activeExe, G_TEXT_SNIPPETS[i].exe_name) != NULL) {
#endif
                Platform_CopyToClipboard(G_TEXT_SNIPPETS[i].text);
            }
        }
    }
    return false;
}
