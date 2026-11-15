#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>
#include "core/types.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_misc.h"

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

#define HOVER_MSG_UNPACK(spec)                                                 \
  const char *key = (spec).key;                                                \
  const char *message = (spec).message;                                        \
  int centerX = (spec).x;                                                      \
  int centerY = (spec).y;                                                      \
  int w = (spec).w;                                                            \
  int h = (spec).h;                                                            \
  int maxWidth = (spec).max_width;                                             \
  int maxHeight = (spec).max_height;                                           \
  ColorRef color = (spec).color;                                               \
  bool no_bg = (spec).no_bg

#define MSG_UNPACK(spec)                                                       \
  const char *message = (spec).message;                                        \
  int centerX = (spec).x;                                                      \
  int centerY = (spec).y;                                                      \
  bool no_bg = (spec).no_bg;                                                   \
  int w = (spec).w;                                                            \
  int h = (spec).h;                                                            \
  int maxWidth = (spec).max_width;                                             \
  int maxHeight = (spec).max_height;                                           \
  double timeout = (spec).timeout;                                             \
  ColorRef color = (spec).color;                                               \
  void (*render)(void) = (spec).render;                                        \
  const char *name = (spec).name

#define ANCHORED_MSG_UNPACK(spec)                                              \
  const char *message = (spec).message;                                        \
  int rect_x = (spec).x;                                                       \
  int rect_y = (spec).y;                                                       \
  int rect_w = (spec).w;                                                       \
  int rect_h = (spec).h;                                                       \
  int h_slot = (spec).h_slot;                                                  \
  int v_slot = (spec).v_slot;                                                  \
  int gap = (spec).gap;                                                        \
  int outward = (spec).outward;                                                \
  int maxWidth = (spec).max_width;                                             \
  int maxHeight = (spec).max_height;                                           \
  double timeout = (spec).timeout;                                             \
  ColorRef color = (spec).color;                                               \
  bool no_bg = (spec).no_bg;                                                   \
  void (*render)(void) = (spec).render;                                        \
  const char *name = (spec).name

#define HTTP_CONNECT_TIMEOUT_S 10
#define HTTP_TIMEOUT_S 30
#define HTTP_MAX_REDIRECTS 10
#define HTTP_DEFAULT_LIMIT ((size_t)64 * 1024 * 1024)

char *Platform_HttpCollect(const char *url, const char *method,
                           const char *content_type, const char *body,
                           size_t max_bytes, size_t *out_len);
bool Platform_HttpSaveToFile(const char *url, const char *path);
bool Platform_HttpThreadStart(void *(*fn)(void *), void *ud);
static inline void Platform_BoundsZero(int *out_x, int *out_y, int *out_w,
                                       int *out_h) {
  if (out_x)
    *out_x = 0;
  if (out_y)
    *out_y = 0;
  if (out_w)
    *out_w = 0;
  if (out_h)
    *out_h = 0;
}

typedef void (*PlatformHttpCallback)(const char *body, bool ok, void *userdata);

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
  size_t limit;
  int overflow;
} HttpBuf;

typedef struct {
  char *url;
  PlatformHttpCallback cb;
  void *userdata;
  char owner[OWNER_PATH_MAX];
} HttpAsyncJob;

typedef enum {
  DEFAULT_BROWSER,
  DEFAULT_TERMINAL,
  DEFAULT_SHELL,
  DEFAULT_APP_COUNT
} DefaultAppKind;

bool Platform_MkdirIfNotExists(const char *path);
bool Platform_TerminateProcessByName(const char *exe_name);
bool Platform_ProcessRunning(const char *name);
bool Platform_FocusWindow(const char *window_name);
WindowHandle Platform_GetWindowByTitle(const char *window_name);
WindowHandle Platform_GetWindowByRootAtom(const char *atom);
bool Platform_SetWindowStringProperty(WindowHandle window, const char *atom,
                                      const char *value);
bool Platform_SetWindowLongProperty(WindowHandle window, const char *atom,
                                    long value);
bool Platform_SendWindowMessage(WindowHandle window, const char *atom,
                                long data);
PlWindowList Platform_GetWindowTitles(void);
PlWindowList Platform_GetWindowNames(void);
bool Platform_QueryBrowser(const char *query);
char *Platform_GetClipboardEntry(void);
bool Platform_PasteContent(const char *content);
int Platform_CopyToClipboard(const char *text);
int Platform_CopyImageToClipboard(const unsigned char *png, size_t len);
int Platform_SetFileToClipboard(const char *path);
char *Platform_GetWorkingDir(void);

long long Platform_GetWallClockMs(void);

void Platform_DismissDisplayMessage(void);
bool Platform_DismissMessage(const char *name);
bool Platform_MessageActive(const char *name);
void Platform_DismissIndefiniteMessage(void);

void Platform_DisplayMessage(PlDisplayMessageSpec spec);
void Platform_DisplayMessageAnchored(PlAnchoredMessageSpec spec);
void Platform_CreateDisplayMessageWindow(PlCreateDisplayMessageWindowSpec spec);
void Platform_HoverMessage(PlHoverMessageSpec spec);
void Platform_HoverMessageRelease(const char *key);
void Platform_MessagePopup_PurgeOwner(const char *path_a, const char *path_b);

FontHandle Platform_CreateFont(int size, const char *face_name);
void Platform_DestroyFont(FontHandle font);
void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight);
int Platform_InitializeTrayIcon(void);
void Platform_PrewarmBackgroundPixmap(void);
void Platform_InitUI(void);
void Platform_InitDataFolder(void);
bool Platform_ResolveAssetFile(const char *stem, const char *rel, char *out,
                               size_t out_len);
typedef struct {
  int item_h;
  int list_top;
} ListboxBackendInit;

bool Platform_ListboxBackendBegin(int item_height_override,
                                  ListboxBackendInit *out);
void Platform_ListboxBackendDrawSelection(int bx, int bw, int y, int item_h,
                                          int fw, int frame_h, bool sel);
void Platform_ListboxBackendEnd(void);
PromptState *Platform_ListboxBackendPromptState(const ListBoxInfo *lb);

void Platform_ShutdownUI(void);
void Platform_RenderThreadMiniSleep(int milliseconds);
void Platform_RenderThreadInit(void);
void Platform_RenderThreadLock(void);
void Platform_RenderThreadUnlock(void);
void Platform_RenderThreadWakeOne(void);
void Platform_RenderThreadWakeAll(void);
void Platform_RenderThreadWaitTimed(long us);
bool Platform_RenderThreadStart(void);
void Platform_RenderThreadJoin(void);
void *Platform_RenderThreadDisplay(void);
char *Platform_RunTerminalCommand(const char *command, bool open_terminal);
char *Platform_RunCommandAndGetOutput(const char *command);
char *Platform_RunArgvCapture(const char *const argv[], const char *stdin_data,
                              int *out_status);
char *Platform_FindExecutable(const char *name);
char *Platform_GetActiveWindowTitle(void);
char *Platform_GetActiveWindowAppId(void);
char *Platform_GetIconPath(char *app_path);
int Platform_GetIconSize(char *icon_path);
bool Platform_PrewarmImage(char *path);
bool Platform_GetMousePos(int *x, int *y);
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
void Platform_ResetUIScale(void);
bool Platform_CloseActiveWindow(void);
bool Platform_ToggleFullscreenActive(void);
bool Platform_MoveActiveWindowToMonitor(bool right);
bool Platform_MoveActiveWindowLeft(void);
bool Platform_InjectKey(int vk, int down);
bool Platform_MoveActiveWindowUp(void);
bool Platform_MoveActiveWindowDown(void);
bool Platform_MoveActiveWindowRight(void);
bool Platform_FocusWindowLeft(void);
bool Platform_FocusWindowUp(void);
bool Platform_FocusWindowDown(void);
bool Platform_FocusWindowRight(void);
bool Platform_MinimizeActiveWindow(void);
bool Platform_LaunchDetached(const char *command);
const char *Platform_GetDefaultApp(DefaultAppKind kind);
bool Platform_ResolveDesktopExec(const char *desktop_id, char *out,
                                 size_t out_size);
typedef struct PlDirIter PlDirIter;
PlDirIter *Platform_DirOpen(const char *dir_path);
bool Platform_DirNext(PlDirIter *it, char *name_out, size_t name_cap,
                      bool *is_dir_out);
void Platform_DirClose(PlDirIter *it);
const char *Platform_GetRealUser(void);
const char *Platform_GetRealHome(void);
bool Platform_DirExists(const char *path);
bool Platform_TempDirPath(char *out, size_t out_len);
bool Platform_FileInfo(const char *path, long long *mtime_out,
                       long long *size_out);
bool Platform_IsRunningAsRoot(void);
bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args);
bool Platform_LaunchBrowserAtUrl(const char *url, bool new_window);
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
bool Platform_FontInitialized(FontHandle font);

void *Platform_ThreadCreate(void *(*fn)(void *), void *arg);
void Platform_ThreadJoin(void *thread);
void *Platform_MutexCreate(void);
void Platform_MutexDestroy(void *mutex);
void Platform_MutexLock(void *mutex);
void Platform_MutexUnlock(void *mutex);
bool Platform_MutexTryLock(void *mutex);

#define PLATFORM_BUILD_ARGV_MAX 48
bool Platform_GetFeatureBuildArgv(const char *src, const char *out_tmp,
                                  const char *argv[], size_t argv_cap);
bool Platform_AtomicRename(const char *old_path, const char *new_path);

typedef struct {
  int edit_cursor_width;
  int edit_cursor_height;
  int editbox_padding;
  int listbox_padding;
  int listbox_item_height;
} PlMetrics;

void Platform_InvalidatePrompt(void);
#endif
