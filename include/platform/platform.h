#ifndef PLATFORM_H
#define PLATFORM_H

#include "core/types.h"

#if !defined(_WIN32)
#include <X11/Xlib.h>
#endif

typedef struct {
    size_t index;
    char original;
} NewlineInfo;

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async);
bool Platform_LaunchApp(const char* path);
bool Platform_LaunchAppAsAdmin(const char* path);
bool Platform_IsCtrlPressed(void);
bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size);
bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command);
bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value);
bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value);
#if !defined(_WIN32)
bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state);
bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state);
bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState* state);
bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state);
#endif

char* Platform_QueryZoxidePath(const char* query);
char Platform_GetLastChar(MappedFile *file);

int Platform_FileInit(const char *filePath, MappedFile *mf);
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
FontHandle Platform_CreateFont(int size, const char* face_name);
#if !defined(_WIN32)
FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name);
#endif
void Platform_DestroyFont(FontHandle font);
void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight);
void Platform_SettingsPopUp(void);
char* Platform_EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight);
char* Platform_SearchPopup(void);
int Platform_InitializeTrayIcon(void);
void Platform_RunHotKeyMessageLoop(void);
void Platform_InitUI(void);
void Platform_ShutdownUI(void);

void Linux_GetActiveWindowTitle(char* buffer, int buffer_size);
int ConvertLinuxKeyToWindowsVK(int key_code);

#endif
