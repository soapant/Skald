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
bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle);
bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix);
int Platform_CollectRunningProcesses(char process_list[][128], int max_count);
bool Platform_TerminateProcessByName(const char* exe_name);
bool Platform_FocusWindow(const char* window_name);
bool Platform_ClickAtPos(int x,int y);
bool Platform_QueryBrowser(char* query);
char* Platform_GetLastClipboardEntry();
bool Platform_PasteContent(char* content);
int Platform_CopyToClipboard(const char* text);
char* Platform_QueryZoxidePath(const char* query);
char Platform_GetLastChar(MappedFile *file);

int Platform_FileAppend(MappedFile *mf, const char *text);
void Platform_FileClose(MappedFile *mf);
Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount);
char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount);
void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount);
int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text);

void Platform_StartUniversalTimer(void);
void Platform_ResetUniversalTimer(void);
void Platform_ModifyUniversalTimer(double seconds_to_add);
double Platform_GetUniversalTimeSeconds(void);

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath);

void Platform_WaitForKeypressAsync(KeyCallback callback);
void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color);
void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color);
FontHandle Platform_CreateFont(int size, const char* face_name);

void Platform_DestroyFont(FontHandle font);
void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight);
void Platform_SettingsPopUp(void);
int Platform_InitializeTrayIcon(void);
void Platform_RunHotKeyMessageLoop(void);
void Platform_InitUI(void);
void Platform_ShutdownUI(void);
char* Platform_RunTerminalCommand(char* command,bool open_terminal);
bool Platform_LaunchFileManagerAtPath(char* path,char* args);
long Platform_GetFileSize(char *filepath);
char* Platform_RegisterDirWatcher(const char* dir_path, void (*callback)(void));
char* Platform_WatchForCFileChanges(const char* dir_path, void (*callback)(void));
char* Platform_GetActiveWindowTitle();
int ConvertLinuxKeyToWindowsVK(int key_code);
const char* Platform_GetRealUser(void);
const char* Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);
bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args);
bool Platform_LaunchBrowserAtUrl(char* url,bool new_window);

#if !defined(_WIN32)
FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name);
bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state);
bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state);
bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState* state);
bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state);
void Platform_DismissMessage(void);
bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command);
bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value);
bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value);
const char* Platform_GetSessionDisplay(void);
const char* Platform_GetSessionXAuthority(void);
const char* Platform_GetRuntimeDir(void);
#endif

#endif
