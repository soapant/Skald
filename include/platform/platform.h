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
bool Platform_IsCtrlPressed(void);
bool Platform_IsShiftPressed(void);
bool Platform_IsAltPressed(void);
bool Platform_IsSuperPressed(void);
bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size);
bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle);
bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix);
bool Platform_TerminateProcessByName(const char* exe_name);
bool Platform_FocusWindow(const char* window_name);
PlWindowList Platform_GetWindowTitles();
PlWindowList Platform_GetWindowNames();
bool Platform_QueryBrowser(char* query);
char* Platform_GetClipboardEntry();
bool Platform_PasteContent(char* content);
int Platform_CopyToClipboard(const char* text);
int Platform_CopyImageToClipboard(const unsigned char* png, size_t len);
int Platform_SetFileToClipboard(const char* path);
char* Platform_QueryZoxidePath(const char* query);
char* Platform_GetWorkingDir();
void *Platform_FeatureWatcherThread(void* arg);

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
long long Platform_GetWallClockMs(void);

void Platform_WaitForKeypressAsync(KeyCallback callback);
void Platform_DisplayMessage(const char* message, int centerX, int centerY, bool no_bg, int w, int h, int maxWidth, int maxHeight, double timeout, ColorRef color, void (*render)(void));
void Platform_DisplayMessageAnchored(const char* message, int rect_x, int rect_y, int rect_w, int rect_h, int h_slot, int v_slot, int gap, int outward, int maxWidth, int maxHeight, double timeout, ColorRef color,bool no_bg);
void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, double timeout, ColorRef color);
void Platform_HoverMessage(const char* key, const char* message, int centerX, int centerY, int w, int h, int maxWidth, int maxHeight, ColorRef color,bool no_bg);
void Platform_HoverMessageRelease(const char* key);
void Platform_HoverWidgetShow(const char* key, void (*render)(void* ud), void* ud, int x, int y, int w, int h, int fps, bool no_bg, unsigned long bg_hex, void (*release)(void* ud), void* release_ud);
void Platform_HoverWidgetHide(const char* key);
FontHandle Platform_CreateFont(int size, const char* face_name);

void Platform_DestroyFont(FontHandle font);
void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight);
void Platform_SettingsPopUp(WindowManager* wm);
int Platform_InitializeTrayIcon(void);
void Platform_InitUI(void);
void Platform_InitDataFolder(void);
bool Platform_GetFeaturePath(const char* feature, const char* name, char* out, size_t out_len);
bool Platform_ResolveDataPath(void *caller, const char* name, char* out, size_t out_len);
bool Platform_WriteTempFile(const char* name, const void* data, size_t len, char* out_path, size_t out_len);
void Platform_ShutdownUI(void);
char* Platform_RunTerminalCommand(char* command,bool open_terminal);
char* Platform_RunCommandAndGetOutput(char* command);
char* Platform_RunArgvCapture(const char* const argv[], const char* stdin_data, int* out_status);
bool Platform_LaunchFileManagerAtPath(char* path,char* args);
long Platform_GetFileSize(char *filepath);
char* Platform_RegisterDirWatcher(const char* dir_path, DirChangeCallback callback);
char* Platform_WatchForCFileChanges(const char* dir_path);
void Platform_CleanupDirWatchers(void);
bool Platform_BuildFeature(const char* src, const char* out);
char* Platform_GetActiveWindowTitle();
char* Platform_GetActiveWindowAppId();
char* Platform_GetIconPath(char* app_path);
int Platform_GetIconSize(char* icon_path);
int ConvertLinuxKeyToWindowsVK(int key_code);
PlColor Platform_GetPixelColor(int x, int y);
bool Platform_MoveToPos(int x, int y);
bool Platform_Click(void);
bool Platform_MouseButtonDown(int button, bool down);
bool Platform_MouseScroll(int notches);
void Platform_ApplyUIScale(void);
float Platform_GetUIScale(void);
int Platform_GetUnscaledFontSize(void);
void Platform_AdjustFontSize(int delta);
void Platform_ResetUIScale(void);
bool Platform_CloseActiveWindow(void);
bool Platform_ToggleFullscreenActive(void);
bool Platform_MoveActiveWindowToMonitor(bool right);
bool Platform_MoveActiveWindowLeft(void);
bool Platform_MoveActiveWindowRight(void);
bool Platform_FocusWindowLeft(void);
bool Platform_FocusWindowRight(void);
bool Platform_MinimizeActiveWindow(void);
bool Platform_LaunchDetached(char* command);
const char* Platform_GetDefaultApp(DefaultAppKind kind);
bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size);
bool Platform_GetAllFilesFromDir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore, bool strip_extension);
const char* Platform_GetRealUser(void);
const char* Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);
bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args);
bool Platform_LaunchBrowserAtUrl(char* url,bool new_window);

void Platform_EmitKey(int vk_code, int down);

bool Platform_TryAcquireSingleInstance(void);
bool Platform_InitDisplaySafe(void);
void Platform_CleanupDisplaySafe(void);
void Platform_InitMainThreadContext(void);
void Platform_FixWorkingDirectory(void);
void Platform_CleanupPlatformResources(void);
void Platform_StartNotificationDaemon(void);

#if !defined(_WIN32)
FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name);
bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state);
void Platform_DismissMessage(void);
void Platform_DismissIndefiniteMessage(void);
bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command);
bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value);
bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value);
const char* Platform_GetSessionDisplay(void);
const char* Platform_GetSessionXAuthority(void);
const char* Platform_GetRuntimeDir(void);
#endif

#endif
