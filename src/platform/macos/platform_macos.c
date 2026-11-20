#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "platform/platform.h"

#define UNUSED(x) (void)(x)

char *Platform_HttpCollect(const char *url, const char *method,
                           const char *content_type, const char *body,
                           size_t max_bytes, size_t *out_len) {
  UNUSED(url);
  UNUSED(method);
  UNUSED(content_type);
  UNUSED(body);
  UNUSED(max_bytes);
  if (out_len)
    *out_len = 0;
  return NULL;
}

bool Platform_HttpSaveToFile(const char *url, const char *path) {
  UNUSED(url);
  UNUSED(path);
  return false;
}

bool Platform_HttpThreadStart(void *(*fn)(void *), void *ud) {
  UNUSED(fn);
  UNUSED(ud);
  return false;
}

bool Platform_MkdirIfNotExists(const char *path) {
  if (!path)
    return false;
  if (mkdir(path, 0755) == 0)
    return true;
  return errno == EEXIST;
}

bool Platform_TerminateProcessByName(const char *exe_name) {
  UNUSED(exe_name);
  return false;
}

bool Platform_ProcessRunning(const char *name) {
  if (!name || !*name)
    return false;
  FILE *fp = popen("ps -A -o comm=", "r");
  if (!fp)
    return false;
  bool found = false;
  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    if (strcasestr(line, name)) {
      found = true;
      break;
    }
  }
  pclose(fp);
  return found;
}

bool Platform_FocusWindow(const char *window_name) {
  UNUSED(window_name);
  return false;
}

WindowHandle Platform_GetWindowByTitle(const char *window_name) {
  UNUSED(window_name);
  return WH_NULL;
}

WindowHandle Platform_GetWindowByRootAtom(const char *atom) {
  UNUSED(atom);
  return WH_NULL;
}

bool Platform_SetWindowStringProperty(WindowHandle window, const char *atom,
                                      const char *value) {
  UNUSED(window);
  UNUSED(atom);
  UNUSED(value);
  return false;
}

bool Platform_SetWindowLongProperty(WindowHandle window, const char *atom,
                                    long value) {
  UNUSED(window);
  UNUSED(atom);
  UNUSED(value);
  return false;
}

bool Platform_SendWindowMessage(WindowHandle window, const char *atom,
                                long data) {
  UNUSED(window);
  UNUSED(atom);
  UNUSED(data);
  return false;
}

WindowList Platform_GetWindowTitles(void) { return (WindowList){0}; }

WindowList Platform_GetWindowIds(void) { return (WindowList){0}; }

bool Platform_QueryBrowser(const char *query) {
  UNUSED(query);
  return false;
}

char *Platform_GetClipboardEntry(void) { return NULL; }

bool Platform_PasteContent(const char *content) {
  UNUSED(content);
  return false;
}

int Platform_CopyToClipboard(const char *text) {
  UNUSED(text);
  return 0;
}

int Platform_CopyImageToClipboard(const unsigned char *png, size_t len) {
  UNUSED(png);
  UNUSED(len);
  return 0;
}

int Platform_SetFileToClipboard(const char *path) {
  UNUSED(path);
  return 0;
}

char *Platform_GetWorkingDir(void) {
  char buf[4096];
  char *cwd = getcwd(buf, sizeof(buf));
  return cwd ? strdup(cwd) : NULL;
}

long long Platform_GetWallClockMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

void Platform_DismissDisplayMessage(void) {}

bool Platform_DismissMessage(const char *name) {
  UNUSED(name);
  return false;
}

bool Platform_MessageActive(const char *name) {
  UNUSED(name);
  return false;
}

void Platform_DismissIndefiniteMessage(void) {}

void Platform_DisplayMessage(DisplayMessageSpec spec) { UNUSED(spec); }

void Platform_DisplayMessageAnchored(AnchoredMessageSpec spec) { UNUSED(spec); }

void Platform_CreateDisplayMessageWindow(CreateDisplayMessageWindowSpec spec) {
  UNUSED(spec);
}

void Platform_HoverMessage(HoverMessageSpec spec) { UNUSED(spec); }

void Platform_HoverMessageRelease(const char *key) { UNUSED(key); }

void Platform_MessagePopup_PurgeOwner(const char *path_a, const char *path_b) {
  UNUSED(path_a);
  UNUSED(path_b);
}

FontHandle Platform_CreateFont(int size, const char *face_name) {
  UNUSED(size);
  UNUSED(face_name);
  return FH_NULL;
}

void Platform_DestroyFont(FontHandle font) { UNUSED(font); }

void Platform_GetTextExtents(FontHandle font, const char *text, int max_width,
                             int *outWidth, int *outHeight) {
  UNUSED(font);
  UNUSED(text);
  UNUSED(max_width);
  if (outWidth)
    *outWidth = 0;
  if (outHeight)
    *outHeight = 0;
}

int Platform_InitializeTrayIcon(void) { return 0; }

void Platform_PrewarmBackgroundPixmap(void) {}

void Platform_InitUI(void) {}

void Platform_InitDataFolder(void) {}

bool Platform_ResolveAssetFile(const char *stem, const char *rel, char *out,
                               size_t out_len) {
  UNUSED(stem);
  UNUSED(rel);
  UNUSED(out);
  UNUSED(out_len);
  return false;
}

bool Platform_ListboxBackendBegin(int item_height_override,
                                  ListboxBackendInit *out) {
  UNUSED(item_height_override);
  UNUSED(out);
  return false;
}

void Platform_ListboxBackendDrawSelection(int bx, int bw, int y, int item_h,
                                          int fw, int frame_h, bool sel) {
  UNUSED(bx);
  UNUSED(bw);
  UNUSED(y);
  UNUSED(item_h);
  UNUSED(fw);
  UNUSED(frame_h);
  UNUSED(sel);
}

void Platform_ListboxBackendEnd(void) {}

PromptState *Platform_ListboxBackendPromptState(const ListBox *lb) {
  UNUSED(lb);
  return NULL;
}

void Platform_ShutdownUI(void) {}

void Platform_RenderThreadMiniSleep(int milliseconds) {
  if (milliseconds <= 0)
    return;
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

void Platform_RenderThreadInit(void) {}

void Platform_RenderThreadLock(void) {}

void Platform_RenderThreadUnlock(void) {}

void Platform_RenderThreadWakeOne(void) {}

void Platform_RenderThreadWakeAll(void) {}

void Platform_RenderThreadWaitTimed(long us) {
  if (us <= 0)
    return;
  struct timespec ts;
  ts.tv_sec = us / 1000000L;
  ts.tv_nsec = (us % 1000000L) * 1000L;
  nanosleep(&ts, NULL);
}

bool Platform_RenderThreadStart(void) { return false; }

void Platform_RenderThreadJoin(void) {}

void *Platform_RenderThreadDisplay(void) { return NULL; }

char *Platform_RunTerminalCommand(const char *command, bool open_terminal) {
  UNUSED(command);
  UNUSED(open_terminal);
  return NULL;
}

char *Platform_RunCommandAndGetOutput(const char *command) {
  UNUSED(command);
  return NULL;
}

char *Platform_RunArgvCapture(const char *const argv[], const char *stdin_data,
                              int *out_status) {
  UNUSED(argv);
  UNUSED(stdin_data);
  if (out_status)
    *out_status = -1;
  return NULL;
}

char *Platform_FindExecutable(const char *name) {
  UNUSED(name);
  return NULL;
}

char *Platform_GetActiveWindowTitle(void) { return NULL; }

char *Platform_GetActiveWindowId(void) { return NULL; }

char *Platform_GetIconPath(char *app_path) {
  UNUSED(app_path);
  return NULL;
}

int Platform_GetIconSize(char *icon_path) {
  UNUSED(icon_path);
  return 0;
}

bool Platform_PrewarmImage(char *path) {
  UNUSED(path);
  return true;
}

bool Platform_GetMousePos(int *x, int *y) {
  UNUSED(x);
  UNUSED(y);
  return false;
}

RgbColor Platform_GetPixelColor(int x, int y) {
  UNUSED(x);
  UNUSED(y);
  return (RgbColor){0, 0, 0};
}

bool Platform_MoveToPos(int x, int y) {
  UNUSED(x);
  UNUSED(y);
  return false;
}

bool Platform_Click(void) { return false; }

bool Platform_ClickButton(int button) {
  UNUSED(button);
  return false;
}

bool Platform_MouseButtonDown(int button, bool down) {
  UNUSED(button);
  UNUSED(down);
  return false;
}

bool Platform_GetVirtualBounds(int *out_x, int *out_y, int *out_w, int *out_h) {
  UNUSED(out_x);
  UNUSED(out_y);
  UNUSED(out_w);
  UNUSED(out_h);
  return false;
}

bool Platform_MouseScroll(int notches) {
  UNUSED(notches);
  return false;
}

void Platform_ApplyUIScale(void) {}

float Platform_GetUIScale(void) { return 1.0f; }

int Platform_GetUnscaledFontSize(void) { return 16; }

int Platform_EditboxPlaceholderOffset(void) { return 2; }

void Platform_ResetUIScale(void) {}

bool Platform_CloseActiveWindow(void) { return false; }

bool Platform_ToggleFullscreenActive(void) { return false; }

bool Platform_MoveActiveWindowToMonitor(bool right) {
  UNUSED(right);
  return false;
}

bool Platform_MoveActiveWindowLeft(void) { return false; }

bool Platform_InjectKey(int vk, int down) {
  UNUSED(vk);
  UNUSED(down);
  return false;
}

bool Platform_MoveActiveWindowUp(void) { return false; }

bool Platform_MoveActiveWindowDown(void) { return false; }

bool Platform_MoveActiveWindowRight(void) { return false; }

bool Platform_FocusWindowLeft(void) { return false; }

bool Platform_FocusWindowUp(void) { return false; }

bool Platform_FocusWindowDown(void) { return false; }

bool Platform_FocusWindowRight(void) { return false; }

bool Platform_MinimizeActiveWindow(void) { return false; }

bool Platform_ToggleMaximizeActiveWindow(void) { return false; }

bool Platform_ToggleAlwaysOnTopActiveWindow(void) { return false; }

bool Platform_RaiseActiveWindow(void) { return false; }

bool Platform_LowerActiveWindow(void) { return false; }

bool Platform_CenterActiveWindow(void) { return false; }

bool Platform_SnapActiveWindow(SnapPreset preset) {
  UNUSED(preset);
  return false;
}

long Platform_GetActiveWindowPid(void) { return 0; }

bool Platform_SwitchToWorkspace(int index) {
  UNUSED(index);
  return false;
}

bool Platform_MoveActiveWindowToWorkspace(int index) {
  UNUSED(index);
  return false;
}

bool Platform_SetActiveWindowOpacity(int percent) {
  UNUSED(percent);
  return false;
}

bool Platform_CloseWindowsByTitle(const char *needle) {
  UNUSED(needle);
  return false;
}

bool Platform_CloseWindowsById(const char *needle) {
  UNUSED(needle);
  return false;
}

bool Platform_LaunchDetached(const char *command) {
  UNUSED(command);
  return false;
}

const char *Platform_GetDefaultApp(DefaultAppKind kind) {
  UNUSED(kind);
  return NULL;
}

bool Platform_ResolveDesktopExec(const char *desktop_id, char *out,
                                 size_t out_size) {
  UNUSED(desktop_id);
  UNUSED(out);
  UNUSED(out_size);
  return false;
}

struct DirIter {
  DIR *dir;
  int dummy;
};

DirIter *Platform_DirOpen(const char *dir_path) {
  UNUSED(dir_path);
  return NULL;
}

bool Platform_DirNext(DirIter *it, char *name_out, size_t name_cap,
                      bool *is_dir_out) {
  UNUSED(it);
  UNUSED(name_out);
  UNUSED(name_cap);
  if (is_dir_out)
    *is_dir_out = false;
  return false;
}

void Platform_DirClose(DirIter *it) {
  if (it)
    free(it);
}

const char *Platform_GetRealUser(void) {
  const struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name)
    return pw->pw_name;
  char *user = getenv("USER");
  return user ? user : "user";
}

const char *Platform_GetRealHome(void) {
  const struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_dir)
    return pw->pw_dir;
  char *home = getenv("HOME");
  return home ? home : "/tmp";
}

bool Platform_DirExists(const char *path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool Platform_TempDirPath(char *out, size_t out_len) {
  const char *dir = getenv("TMPDIR");
  if (!dir || !dir[0])
    dir = "/tmp";
  if (!out || out_len == 0)
    return false;
  size_t len = strlen(dir);
  while (len > 0 && dir[len - 1] == '/')
    len--;
  if (len + 1 > out_len)
    return false;
  memcpy(out, dir, len);
  out[len] = '\0';
  return true;
}

bool Platform_FileInfo(const char *path, long long *mtime_out,
                       long long *size_out) {
  struct stat st;
  if (!path || stat(path, &st) != 0)
    return false;
  if (mtime_out)
    *mtime_out = (long long)st.st_mtime;
  if (size_out)
    *size_out = (long long)st.st_size;
  return true;
}

bool Platform_IsRunningAsRoot(void) { return geteuid() == 0; }

bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args) {
  UNUSED(working_dir);
  UNUSED(extra_args);
  return false;
}

bool Platform_LaunchBrowserAtUrl(const char *url, bool new_window) {
  UNUSED(url);
  UNUSED(new_window);
  return false;
}

Timer Platform_StartTimer(void) {
  Timer t = {0};
  t.ticks = (uint64_t)Platform_TimerNowTicks();
  return t;
}

double Platform_EndTimer(Timer t) {
  int64_t now = Platform_TimerNowTicks();
  return (double)(now - (int64_t)t.ticks) /
         (double)Platform_TimerTickFrequency();
}

int64_t Platform_TimerNowTicks(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t Platform_TimerTickFrequency(void) { return 1000000000LL; }

void Platform_EmitKey(int vk_code, int down) {
  UNUSED(vk_code);
  UNUSED(down);
}

bool Platform_TryAcquireSingleInstance(void) { return true; }

bool Platform_InitDisplaySafe(void) { return true; }

void Platform_CleanupDisplaySafe(void) {}

void Platform_InitMainThreadContext(void) {}

void Platform_FixWorkingDirectory(void) {}

void Platform_CleanupPlatformResources(void) {}

bool Platform_FontInitialized(FontHandle font) {
  UNUSED(font);
  return false;
}

void *Platform_ThreadCreate(void *(*fn)(void *), void *arg) {
  if (!fn)
    return NULL;
  pthread_t th;
  if (pthread_create(&th, NULL, fn, arg) != 0)
    return NULL;
  return (void *)th;
}

void Platform_ThreadJoin(void *thread) {
  if (!thread)
    return;
  pthread_join((pthread_t)thread, NULL);
}

void *Platform_MutexCreate(void) {
  pthread_mutex_t *m = malloc(sizeof(*m));
  if (!m)
    return NULL;
  if (pthread_mutex_init(m, NULL) != 0) {
    free(m);
    return NULL;
  }
  return m;
}

void Platform_MutexDestroy(void *mutex) {
  pthread_mutex_t *m = mutex;
  if (!m)
    return;
  pthread_mutex_destroy(m);
  free(m);
}

void Platform_MutexLock(void *mutex) {
  if (mutex)
    pthread_mutex_lock(mutex);
}

void Platform_MutexUnlock(void *mutex) {
  if (mutex)
    pthread_mutex_unlock(mutex);
}

bool Platform_MutexTryLock(void *mutex) {
  if (!mutex)
    return false;
  return pthread_mutex_trylock(mutex) == 0;
}

bool Platform_GetFeatureBuildArgv(const char *src, const char *out_tmp,
                                  const char *argv[], size_t argv_cap) {
  UNUSED(src);
  UNUSED(out_tmp);
  UNUSED(argv);
  UNUSED(argv_cap);
  return false;
}

bool Platform_AtomicRename(const char *old_path, const char *new_path) {
  return rename(old_path, new_path) == 0;
}

void Platform_InvalidatePrompt(void) {}
