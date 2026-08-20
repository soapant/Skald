#include "platform/platform.h"
#include "common.h"
#include "core/modal.h"
#include "core/types.h"
#include "core/timer.h"
#include "helpers.h"
#include "bindings/bindings.h"
#include "bindings/hotkey_definitions.h"
#include "bindings/vim_motions.h"
#include "features/focus_switcher.h"
#include "features/music_player.h"
#include "features/video_player.h"
#include "features/process_killer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/input.h>

void Platform_WaitForKeypressAsync(KeyCallback callback) {
    (void)callback;
    fprintf(stderr, "[PLATFORM] Platform_WaitForKeypressAsync not implemented yet\n");
}

char* Platform_PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback) {
    (void)window_list; (void)window_title; (void)count; (void)height; (void)width; (void)item_height;
    (void)refresh_callback; (void)enter_callback; (void)ctrl_enter_callback; (void)tab_callback;
    (void)copy_callback; (void)render_callback; (void)edit_callback; (void)filter_callback; (void)help_callback;
    fprintf(stderr, "[PLATFORM] Platform_PromptUser not implemented yet\n");
    return NULL;
}

char* Platform_SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height) {
    (void)prompt; (void)window_title; (void)height; (void)width; (void)item_height;
    fprintf(stderr, "[PLATFORM] Platform_SimplePromptUser not implemented yet\n");
    return NULL;
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout) {
    (void)message; (void)centerX; (void)centerY; (void)maxWidth; (void)maxHeight; (void)timeout;
    fprintf(stderr, "[PLATFORM] Platform_DisplayMessage not implemented yet\n");
}

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout) {
    (void)message; (void)centerX; (void)centerY; (void)maxWidth; (void)maxHeight; (void)timeout;
    fprintf(stderr, "[PLATFORM] Platform_CreateDisplayMessageWindow not implemented yet\n");
}

void Platform_SettingsPopUp(void) {
    fprintf(stderr, "[PLATFORM] Platform_SettingsPopUp not implemented yet\n");
}


char* Platform_EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight) {
    (void)initial_text; (void)centerX; (void)centerY; (void)maxWidth; (void)maxHeight;
    fprintf(stderr, "[PLATFORM] Platform_EditTextPopup not implemented yet\n");
    return NULL;
}

char* Platform_SearchPopup(void) {
    fprintf(stderr, "[PLATFORM] Platform_SearchPopup not implemented yet\n");
    return NULL;
}

int Platform_InitializeTrayIcon(void) {
    return -1;
}
#define MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} SystemKeyboard;

static SystemKeyboard active_kbs[MAX_KEYBOARDS];
static int active_kb_count = 0;

void ScanAndHookKeyboards(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && active_kb_count < MAX_KEYBOARDS) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);

            int fd = open(full_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
            if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0) {
                if (ev_bits[0] & (1 << EV_KEY)) {
                    active_kbs[active_kb_count].fd = fd;
                    strncpy(active_kbs[active_kb_count].path, full_path, 255);
                    active_kb_count++;
                } else {
                    close(fd);
                }
            } else {
                close(fd);
            }
        }
    }
    closedir(dir);
}


bool g_Linux_CtrlHeld  = false;
bool g_Linux_AltHeld   = false;
bool g_Linux_ShiftHeld = false;

void HandleGlobalKeyEvent(int key_code, int value) {
    bool is_pressed = (value == 1);
    bool is_released = (value == 0);
    
    if (key_code == KEY_LEFTCTRL || key_code == KEY_RIGHTCTRL) {
        g_Linux_CtrlHeld = is_pressed;
    }
    if (key_code == KEY_LEFTALT || key_code == KEY_RIGHTALT) {
        g_Linux_AltHeld = is_pressed;
    }
    if (key_code == KEY_LEFTSHIFT || key_code == KEY_RIGHTSHIFT) {
        g_Linux_ShiftHeld = is_pressed;
    }
    
    
    if (!is_pressed) return;
    
    
    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
        Platform_ResetUniversalTimer();
        printf("SUPPOSED TO CLEAR FILE\n");
    }

    char fgTitle[256] = {0};
    Linux_GetActiveWindowTitle(fgTitle, sizeof(fgTitle));

    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    
    int modifiers = HOTKEY_MOD_NONE;
    if (g_Linux_AltHeld)   modifiers |= HOTKEY_MOD_ALT;
    if (g_Linux_ShiftHeld) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_Linux_CtrlHeld)  modifiers |= HOTKEY_MOD_CTRL;

    WinTimer timer = TimerStart();
    if (ProcessHotkey(G_HOTKEY_BINDINGS, HOTKEY_BINDING_COUNT, 
          pressed_vk, modifiers, fgTitle)) {
    }
    TimerEnd(timer, "ProcessHotkey");

    if (g_Linux_ShiftHeld && !IsInsertMode() && WH_TO_HWND(hPopupWnd) != NULL) {
        if (WH_TO_HWND(g_hEditPopup) == NULL) {
            if (pressed_vk == COMMAND_MODE_KEY) {
                SetMode(MODE_INSERT);
                char* output = Platform_EditTextPopup("", 960, 540, 1000, 300);
                if (output) {
                    HandleCommandExecution(output);
                }
                return;
            }
        }
    }
    
    if (WH_TO_HWND(g_hSearchPopup) == NULL && WH_TO_HWND(hPopupWnd) != NULL && IsNormalMode()) {
        if (pressed_vk == SEARCH_PROGRAM_KEY) {
            Platform_SearchPopup();
        }
    }

    WinTimer timer2 = TimerStart();
    if (g_Linux_CtrlHeld) {
        if (pressed_vk == HELP_SCREEN_KEY) {
            if (g_help_callback != NULL) {
                g_help_callback();
                TimerEnd(timer2, "HelpScreen");
            }
        }
    }
    TimerEnd(timer2, "HelpScreenCheck");

    WinTimer timer3 = TimerStart();
    if (IsNormalMode()) {
        if (g_waiting_for_key) {
            g_waiting_for_key = false;
            if (g_key_callback) {
                g_key_callback(pressed_vk); // Pass converted key variable down
                TimerEnd(timer3, "KeyCallback");
            }
            return;
        }       
    }
    TimerEnd(timer3, "NormalModeCheck");

    MotionAction vimAction = MOTION_NONE;
    const char* matchedSequence = NULL;
    int typedCount = 0;

    WinTimer timer4 = TimerStart();
    if (CheckVimMotion(pressed_vk, modifiers, &vimAction, &matchedSequence, &typedCount)) {
        ExecuteMotion(vimAction, matchedSequence, typedCount);
        TimerEnd(timer4, "ExecuteMotion");
        return;
    }
    TimerEnd(timer4, "CheckVimMotion");

    WinTimer timer5 = TimerStart();

    if (CheckTextSnippetBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer5, "CheckTextSnippetBindings");

    if (CheckMacroBindings(pressed_vk, modifiers)) {
        return;
    }

    WinTimer timer7 = TimerStart();
    if (CheckAppHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer7, "CheckAppHotkeyBindings");

    WinTimer timer8 = TimerStart();
    if (CheckMiscHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer8, "CheckMiscHotkeyBindings");

    WinTimer timer9 = TimerStart();
    if (CheckQuickLinkHotkeyBindings(pressed_vk, modifiers)) {
        return;
    }
    TimerEnd(timer9, "CheckQuickLinkHotkeyBindings");

    WinTimer timer10 = TimerStart();
    if (HandleFocusSwitcher(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer10, "HandleFocusSwitcher");

    WinTimer timer11 = TimerStart();
    if (HandleMusicPlayer(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer11, "HandleMusicPlayer");

    WinTimer timer12 = TimerStart();
    if (HandleVideoPlayer(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer12, "HandleVideoPlayer");

    WinTimer timer13 = TimerStart();
    if (HandleBulkKillProcess(pressed_vk, modifiers, fgTitle)) {
        return;
    }
    TimerEnd(timer13, "HandleBulkKillProcess");
}

void Platform_RunHotKeyMessageLoop(void) {
    ScanAndHookKeyboards();
    if (active_kb_count == 0) {
        fprintf(stderr, "[ERROR] No structural keyboard nodes located under /dev/input/\n");
        return;
    }

    printf("[SYSTEM] Low-Level Kernel Hook installed across %d input streams.\n", active_kb_count);

    struct pollfd *fds = calloc(active_kb_count, sizeof(struct pollfd));
    for (int i = 0; i < active_kb_count; i++) {
        fds[i].fd = active_kbs[i].fd;
        fds[i].events = POLLIN;
    }

    struct input_event ev;

    while (true) {
        int poll_res = poll(fds, active_kb_count, -1);
        if (poll_res < 0) break;

        for (int i = 0; i < active_kb_count; i++) {
            if (fds[i].revents & POLLIN) {
                while (read(fds[i].fd, &ev, sizeof(struct input_event)) > 0) {
                    if (ev.type == EV_KEY) {
                        HandleGlobalKeyEvent(ev.code, ev.value);
                    }
                }
            }
        }
    }

    for (int i = 0; i < active_kb_count; i++) {
        close(active_kbs[i].fd);
    }
    free(fds);
}

void Platform_InitUI(void) {
    fprintf(stderr, "[PLATFORM] Platform_InitUI not implemented yet\n");
}

void Platform_ShutdownUI(void) {
    fprintf(stderr, "[PLATFORM] Platform_ShutdownUI not implemented yet\n");
}
