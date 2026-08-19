#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>

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
    fprintf(stderr, "[PLATFORM] Platform_InitializeTrayIcon not implemented yet\n");
    return -1;
}

void Platform_RunHotKeyMessageLoop(void) {
    fprintf(stderr, "[PLATFORM] Platform_RunHotKeyMessageLoop not implemented yet\n");
}

void Platform_InitUI(void) {
    fprintf(stderr, "[PLATFORM] Platform_InitUI not implemented yet\n");
}

void Platform_ShutdownUI(void) {
    fprintf(stderr, "[PLATFORM] Platform_ShutdownUI not implemented yet\n");
}
