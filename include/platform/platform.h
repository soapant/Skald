#ifndef PLATFORM_H
#define PLATFORM_H

#include "core/types.h"

typedef struct {
    size_t index;
    char original;
} NewlineInfo;

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async);
bool Platform_LaunchApp(const char* path);
bool Platform_LaunchAppAsAdmin(const char* path);
bool Platform_IsCtrlPressed(void);

char* Platform_QueryZoxidePath(const char* query);

int Platform_FileInit(const char *filePath, MappedFile *mf, size_t initialPadding);
int Platform_FileAppend(MappedFile *mf, const char *text);
void Platform_FileClose(MappedFile *mf);
Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount);
char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount);
void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount);
void Platform_SanitizeFile(const char *filePath);
int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text);

int Platform_CopyToClipboard(const char* text);

void Platform_StartUniversalTimer(void);
void Platform_ResetUniversalTimer(void);
void Platform_ModifyUniversalTimer(double seconds_to_add);
double Platform_GetUniversalTimeSeconds(void);

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath);

void Platform_WaitForKeypressAsync(KeyCallback callback);
char* Platform_PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback);
char* Platform_SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);
void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout);
void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout);
void Platform_SettingsPopUp(void);
char* Platform_EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight);
char* Platform_SearchPopup(void);
int Platform_InitializeTrayIcon(void);
void Platform_RunHotKeyMessageLoop(void);
void Platform_InitUI(void);
void Platform_ShutdownUI(void);

#endif