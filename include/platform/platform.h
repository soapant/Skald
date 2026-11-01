#ifndef PLATFORM_H
#define PLATFORM_H

#include "core/types.h"

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

typedef struct {
  const char *message;
  int x, y, w, h;
  int h_slot, v_slot;
  int gap;
  int outward;
  int max_width, max_height;
  double timeout;
  ColorRef color;
  bool no_bg;
  void (*render)(void);
  const char *name;
} PlAnchoredMessageSpec;

typedef struct {
  const char *key;
  void (*render)(void *ud);
  void *ud;
  int x, y, w, h;
  int fps;
  bool no_bg;
  unsigned long bg_hex;
  void (*release)(void *ud);
  void *release_ud;
} PlHoverWidgetShowSpec;

typedef struct {
  const char *message;
  int x, y;
  int max_width, max_height;
  double timeout;
  ColorRef color;
  bool no_bg;
  void (*render)(void);
  const char *name;
} PlCreateDisplayMessageWindowSpec;

typedef struct {
  const char *key;
  const char *message;
  int x, y, w, h;
  int max_width, max_height;
  ColorRef color;
  bool no_bg;
} PlHoverMessageSpec;

typedef struct {
  const char *message;
  int x, y;
  bool no_bg;
  int w, h;
  int max_width, max_height;
  double timeout;
  ColorRef color;
  void (*render)(void);
  const char *name;
} PlDisplayMessageSpec;

typedef struct {
  const char *dir_path;
  char **list_of_allowed_extensions;
  ListOfFiles *list_of_files;
  char **list_of_names_to_ignore;
  bool strip_extension;
  int max_depth;
} PlGetAllFilesFromDirSpec;

#define HTTP_CONNECT_TIMEOUT_S 10
#define HTTP_TIMEOUT_S 30
#define HTTP_MAX_REDIRECTS 10
#define HTTP_DEFAULT_LIMIT ((size_t)64 * 1024 * 1024)

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
  size_t limit;
  int overflow;
} HttpBuf;

#if defined(__linux__)
void Platform_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd,
                          Visual *vis, Colormap cmap);
#endif
bool Platform_IsCtrlPressed(void);
bool Platform_IsShiftPressed(void);
bool Platform_IsAltPressed(void);
bool Platform_IsSuperPressed(void);
bool Platform_ExpandPath(const char *template_path, char *out, size_t out_size);
bool Platform_StringContainsCaseInsensitive(const char *haystack,
                                            const char *needle);
bool Platform_StringStartsWithCaseInsensitive(const char *str,
                                              const char *prefix);
bool Platform_TerminateProcessByName(const char *exe_name);
bool Platform_FocusWindow(const char *window_name);
PlWindowList Platform_GetWindowTitles();
PlWindowList Platform_GetWindowNames();
bool Platform_QueryBrowser(char *query);
char *Platform_GetClipboardEntry();
bool Platform_PasteContent(char *content);
int Platform_CopyToClipboard(const char *text);
int Platform_CopyImageToClipboard(const unsigned char *png, size_t len);
int Platform_SetFileToClipboard(const char *path);
char *Platform_GetWorkingDir();

void Platform_StartUniversalTimer(void);
void Platform_ResetUniversalTimer(void);
void Platform_ModifyUniversalTimer(double seconds_to_add);
double Platform_GetUniversalTimeSeconds(void);
long long Platform_GetWallClockMs(void);

void Platform_WaitForKeypressAsync(KeyCallback callback);
bool Platform_HandleWaitingForKey(unsigned int vk);
void Platform_DismissDisplayMessage(void);
bool Platform_DismissMessage(const char *name);
bool Platform_MessageActive(const char *name);

void Platform_DisplayMessage(PlDisplayMessageSpec spec);
void Platform_DisplayMessageAnchored(PlAnchoredMessageSpec spec);
void Platform_CreateDisplayMessageWindow(PlCreateDisplayMessageWindowSpec spec);
void Platform_HoverMessage(PlHoverMessageSpec spec);
void Platform_HoverWidgetShow(PlHoverWidgetShowSpec spec);
void Platform_HoverMessageRelease(const char *key);
void Platform_HoverWidgetHide(const char *key);

FontHandle Platform_CreateFont(int size, const char *face_name);
void Platform_DestroyFont(FontHandle font);
void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight);
int Platform_InitializeTrayIcon(void);
void Platform_PrewarmBackgroundPixmap(void);
void Platform_InitUI(void);
void Platform_InitDataFolder(void);
bool Platform_GetFeaturePath(const char *feature, const char *name, char *out,
                             size_t out_len);
bool Platform_FeatureDataDirExists(const char *stem);
bool Platform_ResolveAssetFile(const char *stem, const char *rel, char *out,
                               size_t out_len);
bool Platform_ResolveDataPath(void *caller, const char *name, char *out,
                              size_t out_len);
bool Platform_ResolveAssetPath(void *caller, const char *name, char *out,
                               size_t out_len);
bool Platform_WriteTempFile(const char *name, const void *data, size_t len,
                            char *out_path, size_t out_len);
void Platform_ShutdownUI(void);
char *Platform_RunTerminalCommand(char *command, bool open_terminal);
#define PLATFORM_RUN_PRIO_NORMAL 0
#define PLATFORM_RUN_PRIO_LOW 1
char *Platform_RunCommandAndGetOutput(char *command, int priority);
char *Platform_RunArgvCapture(const char *const argv[], const char *stdin_data,
                              int *out_status);
long Platform_GetFileSize(char *filepath);
char *Platform_GetActiveWindowTitle();
char *Platform_GetActiveWindowAppId();
char *Platform_GetIconPath(char *app_path);
int Platform_GetIconSize(char *icon_path);
int ConvertLinuxKeyToWindowsVK(int key_code);
PlColor Platform_GetPixelColor(int x, int y);
bool Platform_MoveToPos(int x, int y);
bool Platform_Click(void);
bool Platform_ClickButton(int button);
bool Platform_MouseButtonDown(int button, bool down);
bool Platform_GetVirtualBounds(int *out_x, int *out_y, int *out_w, int *out_h);
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
bool Platform_LaunchDetached(char *command);
const char *Platform_GetDefaultApp(DefaultAppKind kind);
bool Platform_ResolveDesktopExec(const char *desktop_id, char *out,
                                 size_t out_size);
bool Platform_GetAllFilesFromDir_impl(PlGetAllFilesFromDirSpec spec);
#define Platform_GetAllFilesFromDir(...)                                       \
  Platform_GetAllFilesFromDir_impl((PlGetAllFilesFromDirSpec){__VA_ARGS__})
const char *Platform_GetRealUser(void);
const char *Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);
bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args);
bool Platform_LaunchBrowserAtUrl(char *url, bool new_window);
PlTimer Platform_StartTimer(void);
double Platform_EndTimer(PlTimer t);
int64_t Platform_TimerNowTicks(void);
int64_t Platform_TimerTickFrequency(void);

void Platform_EmitKey(int vk_code, int down);

bool Platform_TryAcquireSingleInstance(void);
bool Platform_InitDisplaySafe(void);
void Platform_CleanupDisplaySafe(void);
void Platform_InitMainThreadContext(void);
void Platform_FixWorkingDirectory(void);
void Platform_CleanupPlatformResources(void);
bool Platform_FontInitalized(FontHandle font);

typedef void (*PlatformHttpCallback)(const char *body, bool ok, void *userdata);
char *Platform_HttpGet(const char *url);
char *Platform_HttpGetLimited(const char *url, size_t max_bytes,
                              size_t *out_len);
bool Platform_HttpDownload(const char *url, const char *save_path);
char *Platform_HttpPost(const char *url, const char *content_type,
                        const char *body);
void Platform_HttpGetAsync(const char *url, PlatformHttpCallback cb,
                           void *userdata);

void *Platform_ThreadCreate(void *(*fn)(void *), void *arg);
void Platform_ThreadJoin(void *thread);
void *Platform_MutexCreate(void);
void Platform_MutexDestroy(void *mutex);
void Platform_MutexLock(void *mutex);
void Platform_MutexUnlock(void *mutex);
bool Platform_MutexTryLock(void *mutex);

bool Platform_GetFeatureBuildCommand(const char *src, const char *out_tmp,
                                     char *cmd, size_t cmd_size);
bool Platform_AtomicRename(const char *old_path, const char *new_path);
#if defined(__linux__)
FontHandle Platform_CreateFontOnDisplay(Display *dpy, int size,
                                        const char *face_name);
void Platform_DismissIndefiniteMessage(void);
const char *Platform_GetSessionDisplay(void);
const char *Platform_GetSessionXAuthority(void);
const char *Platform_GetRuntimeDir(void);
#endif

#endif
