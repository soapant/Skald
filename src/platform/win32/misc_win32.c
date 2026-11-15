#include "helpers/helpers_text.h"
#include <windows.h>
#include "helpers/helpers_fs.h"
#include <winreg.h>
#include <ctype.h>
#include "config_values.h"
#include "core/types.h"
#include "deps/log.h"
#include <tlhelp32.h>
#include <shlwapi.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "platform/platform.h"
#include "helpers/win32/raw_default_apps_win32.h"
#include "helpers/helpers_misc.h"
#include <time.h>

bool Platform_MkdirIfNotExists(const char *path) {

  if (!path)
    return false;
  if (CreateDirectoryA(path, NULL))
    return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool Platform_DirExists(const char *path) {
  if (!path)
    return false;
  DWORD attrs = GetFileAttributesA(path);
  return attrs != INVALID_FILE_ATTRIBUTES &&
         (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Platform_FileInfo(const char *path, long long *mtime_out,
                       long long *size_out) {
  struct _stat64 st;
  if (!path || _stat64(path, &st) != 0)
    return false;
  if (mtime_out)
    *mtime_out = (long long)st.st_mtime;
  if (size_out)
    *size_out = (long long)st.st_size;
  return true;
}

bool Platform_TempDirPath(char *out, size_t out_len) {
  if (!out || out_len == 0)
    return false;
  char dir[MAX_PATH];
  DWORD dir_n = GetTempPathA(sizeof(dir), dir);
  if (dir_n == 0 || dir_n >= sizeof(dir))
    return false;
  size_t dl = strlen(dir);
  while (dl > 0 && (dir[dl - 1] == '\\' || dir[dl - 1] == '/'))
    dir[--dl] = '\0';
  snprintf(out, out_len, "%s", dir);
  return true;
}

long long Platform_GetWallClockMs(void) {

  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER t;
  t.LowPart = ft.dwLowDateTime;
  t.HighPart = ft.dwHighDateTime;
  return (long long)((t.QuadPart - 116444736000000000ULL) / 10000ULL);
}

bool Platform_ProcessRunning(const char *name) {
  if (!name || !*name) {
    return false;
  }
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) {
    return false;
  }
  PROCESSENTRY32 pe = {0};
  pe.dwSize = sizeof(pe);
  bool found = false;
  if (Process32First(snap, &pe)) {
    do {
      if (string_contains_ci(pe.szExeFile, name)) {
        found = true;
        break;
      }
    } while (Process32Next(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

bool Platform_TerminateProcessByName(const char *exe_name) {
  if (!exe_name || !*exe_name)
    return false;

  if (!is_safe_process_name(exe_name)) {
    log_error("Platform_TerminateProcessByName: refusing unsafe name '%s'",
              exe_name);
    return false;
  }

  char cmd[512];
  const char *ext = strrchr(exe_name, '.');

  if (!ext) {
    snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s.exe*\"", exe_name);
  } else {
    snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", exe_name);
  }

  Platform_RunTerminalCommand(cmd, false);
  log_info("Platform_TerminateProcessByName exe_name='%s'", exe_name);
  return true;
}

static bool win32_resolve_browser(char *out, size_t cap) {
  if (!UsedBrowser || !UsedBrowser[0])
    return false;
  if (SearchPathA(NULL, UsedBrowser, ".exe", (DWORD)cap, out, NULL) ||
      SearchPathA(NULL, UsedBrowser, NULL, (DWORD)cap, out, NULL))
    return true;
  char with_exe[MAX_PATH];
  snprintf(with_exe, sizeof(with_exe), "%s.exe", UsedBrowser);
  return SearchPathA(NULL, with_exe, NULL, (DWORD)cap, out, NULL) != 0;
}

bool Platform_LaunchBrowserAtUrl(const char *url, bool new_window) {
  char full_url[2048] = {0};
  if (!url || !url[0])
    full_url[0] = '\0';
  else if (strstr(url, "://"))
    snprintf(full_url, sizeof(full_url), "%s", url);
  else
    snprintf(full_url, sizeof(full_url), "https://%s", url);
  if (full_url[0] == '\0') {
    if (UsedBrowser && UsedBrowser[0]) {
      char browser_path[MAX_PATH] = {0};
      if (win32_resolve_browser(browser_path, sizeof(browser_path))) {
        HINSTANCE r = ShellExecuteA(NULL, "open", browser_path, NULL, NULL,
                                    SW_SHOWNORMAL);
        if ((intptr_t)r > 32) {
          log_info("Platform_LaunchBrowserAtUrl via %s (no url)", browser_path);
          return true;
        }
      }
      HINSTANCE r2 =
          ShellExecuteA(NULL, "open", UsedBrowser, NULL, NULL, SW_SHOWNORMAL);
      if ((intptr_t)r2 > 32) {
        log_info("Platform_LaunchBrowserAtUrl via UsedBrowser (no url)");
        return true;
      }
    }
    if (win32_launch_default_browser_blank())
      return true;
    log_info(
        "Platform_LaunchBrowserAtUrl no url, no browser could be launched");
    return false;
  }
  if (new_window && UsedBrowser && UsedBrowser[0]) {
    char browser_path[MAX_PATH] = {0};
    if (win32_resolve_browser(browser_path, sizeof(browser_path))) {
      char *exe = browser_path;
      char params[4096];
      snprintf(params, sizeof(params), "--new-window \"%s\"", full_url);
      HINSTANCE r =
          ShellExecuteA(NULL, "open", exe, params, NULL, SW_SHOWNORMAL);
      if ((intptr_t)r > 32) {
        log_info("Platform_LaunchBrowserAtUrl via %s new_window url='%s'", exe,
                 full_url);
        return true;
      }
    }
    char params2[4096];
    snprintf(params2, sizeof(params2), "--new-window \"%s\"", full_url);
    HINSTANCE r2 =
        ShellExecuteA(NULL, "open", UsedBrowser, params2, NULL, SW_SHOWNORMAL);
    if ((intptr_t)r2 > 32) {
      log_info(
          "Platform_LaunchBrowserAtUrl via UsedBrowser new_window url='%s'",
          full_url);
      return true;
    }
  }
  HINSTANCE r3 =
      ShellExecuteA(NULL, "open", full_url, NULL, NULL, SW_SHOWNORMAL);
  if ((intptr_t)r3 > 32) {
    log_info("Platform_LaunchBrowserAtUrl via default url='%s' new_window=%d",
             full_url, new_window);
    return true;
  }
  char command[2048];
  snprintf(command, sizeof(command), "start \"\" \"%s\"", full_url);
  log_info("Platform_LaunchBrowserAtUrl fallback url='%s' new_window=%d",
           full_url, new_window);
  return Platform_RunTerminalCommand(command, false);
}

bool Platform_QueryBrowser(const char *query) {
  if (!query || !query[0])
    return false;

  char url[4096];
  url_build_search_url(UsedSearchEngine, query, url, sizeof(url));

  char command[4096];
  snprintf(command, sizeof(command), "start \"\" \"%s\"", url);
  log_info("Platform_QueryBrowser query='%s'", query);
  return Platform_RunTerminalCommand(command, false);
}

char *Platform_GetClipboardEntry(void) {
  char *result = NULL;

  if (!OpenClipboard(NULL))
    return NULL;

  HANDLE hClipData = GetClipboardData(CF_UNICODETEXT);
  if (hClipData == NULL) {
    CloseClipboard();
    return NULL;
  }

  wchar_t *pClipText = (wchar_t *)GlobalLock(hClipData);
  if (pClipText == NULL) {
    CloseClipboard();
    return NULL;
  }
  size_t clipBytes = GlobalSize(hClipData);
  DWORD wideCharBuf = (DWORD)(clipBytes / sizeof(wchar_t));
  const wchar_t *clipEnd =
      (wchar_t *)memchr(pClipText, 0, wideCharBuf * sizeof(wchar_t));
  int wideCharLen = clipEnd ? (int)(clipEnd - pClipText) : (int)wideCharBuf;
  int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pClipText, wideCharLen, NULL, 0,
                                    NULL, NULL);

  if (utf8Len > 0) {
    result = (char *)malloc(utf8Len + 1);
    if (result != NULL) {
      WideCharToMultiByte(CP_UTF8, 0, pClipText, wideCharLen, result, utf8Len,
                          NULL, NULL);
      result[utf8Len] = '\0';
    }
  }

  GlobalUnlock(hClipData);
  CloseClipboard();
  return result;
}

bool Platform_PasteContent(const char *content) {
  if (!content || !content[0])
    return false;

  int wchars = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
  if (wchars <= 1)
    return false;
  wchar_t *wbuf = (wchar_t *)malloc((size_t)wchars * sizeof(wchar_t));
  if (!wbuf)
    return false;
  MultiByteToWideChar(CP_UTF8, 0, content, -1, wbuf, wchars);
  int code_units = wchars - 1; /* don't paste the trailing NUL */

  INPUT *inputs = (INPUT *)calloc((size_t)code_units * 2, sizeof(INPUT));
  if (!inputs) {
    free(wbuf);
    return false;
  }
  for (int i = 0; i < code_units; i++) {
    INPUT *down = &inputs[2 * i];
    INPUT *up = &inputs[2 * i + 1];
    down->type = INPUT_KEYBOARD;
    down->ki.wVk = 0;
    down->ki.wScan = wbuf[i];
    down->ki.dwFlags = KEYEVENTF_UNICODE;
    up->type = INPUT_KEYBOARD;
    up->ki.wVk = 0;
    up->ki.wScan = wbuf[i];
    up->ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
  }

  UINT total = (UINT)code_units * 2;
  UINT result = SendInput(total, inputs, sizeof(INPUT));
  bool success = (result == total);

  free(inputs);
  free(wbuf);
  log_info("Platform_PasteContent content length=%zu", strlen(content));
  return success;
}
const char *Platform_GetRealHome(void) {

  static char buf[512] = {0};
  buf[0] = '\0';

  char user_profile[MAX_PATH];
  memset(user_profile, 0, sizeof(user_profile));
  DWORD n = GetEnvironmentVariableA("USERPROFILE", user_profile, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    snprintf(buf, sizeof(buf), "%s", user_profile);
    return buf;
  }
  if (n >= MAX_PATH)
    log_warn("Platform_GetRealHome: USERPROFILE truncated (%lu)", n);

  char drive[MAX_PATH];
  memset(drive, 0, sizeof(drive));
  n = GetEnvironmentVariableA("HOMEDRIVE", drive, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    char path[MAX_PATH];
    memset(path, 0, sizeof(path));
    DWORD m = GetEnvironmentVariableA("HOMEPATH", path, MAX_PATH);
    if (m > 0 && m < MAX_PATH) {
      snprintf(buf, sizeof(buf), "%s%s", drive, path);
      return buf;
    }
  }
  return buf;
}

bool Platform_IsRunningAsRoot(void) {
  HANDLE token = NULL;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  TOKEN_ELEVATION elevation;
  DWORD size = sizeof(elevation);
  BOOL ok = GetTokenInformation(token, TokenElevation, &elevation,
                                sizeof(elevation), &size);
  CloseHandle(token);
  return ok && elevation.TokenIsElevated;
}

const char *Platform_GetRealUser(void) {
  static char buf[256];
  DWORD n = sizeof(buf);
  memset(buf, 0, sizeof(buf));
  if (GetUserNameA(buf, &n) && buf[0])
    return buf;
  const char *u = getenv("USERNAME");
  if (u && u[0]) {
    snprintf(buf, sizeof(buf), "%s", u);
    return buf;
  }
  return NULL;
}

static bool win32_compiler_probe(const char *cc) {
  char tmp_dir[MAX_PATH], src[MAX_PATH], dll[MAX_PATH];
  DWORD n = GetTempPathA(MAX_PATH, tmp_dir);
  if (n == 0 || n >= MAX_PATH)
    return false;
  DWORD pid = GetCurrentProcessId();
  snprintf(src, sizeof(src), "%sskald_cc_probe_%lu.c", tmp_dir, pid);
  snprintf(dll, sizeof(dll), "%sskald_cc_probe_%lu.dll", tmp_dir, pid);
  FILE *f = fopen(src, "wb");
  if (!f)
    return false;
  fputs("__declspec(dllexport) int skald_cc_probe(void) { return 42; }\n", f);
  fclose(f);
  const char *argv[] = {cc, "-shared", "-o", dll, src, "-lcomctl32", NULL};
  int status = -1;
  char *out = Platform_RunArgvCapture(argv, NULL, &status);
  bool ok = out != NULL && status == 0 &&
            GetFileAttributesA(dll) != INVALID_FILE_ATTRIBUTES;
  free(out);
  DeleteFileA(src);
  DeleteFileA(dll);
  return ok;
}

static const char *win32_compiler_found(void) {
  static const char *cc = NULL;
  if (!cc) {
    const char *candidates[] = {"clang", "gcc", NULL};
    cc = compiler_find_usable(candidates, win32_compiler_probe);
  }
  return cc;
}

bool Platform_GetFeatureBuildArgv(const char *src, const char *out_tmp,
                                  const char *argv[], size_t argv_cap) {
  const char *cc = win32_compiler_found();
  if (!cc || argv_cap < 30)
    return false;
  size_t n = compiler_argv_base(cc, argv);
  argv[n++] = "-pipe";
  argv[n++] = "-shared";
  argv[n++] = "-o";
  argv[n++] = out_tmp;
  argv[n++] = src;
  argv[n++] = "build/main.lib.a";
  argv[n++] = "-lcomctl32";
  argv[n++] = "-lgdiplus";
  argv[n++] = "-lgdi32";
  argv[n++] = "-lshlwapi";
  argv[n++] = "-lshell32";
  argv[n++] = "-lwinmm";
  argv[n++] = "-lole32";
  argv[n++] = "-luuid";
  argv[n++] = "-lmsimg32";
  argv[n++] = "-ldwmapi";
  argv[n++] = "-lpsapi";
  argv[n++] = "-lcrypt32";
  argv[n++] = "-lcomdlg32";
  argv[n++] = "-ladvapi32";
  argv[n++] = "-ldxva2";
  argv[n++] = "-luser32";
  argv[n++] = "-lwinhttp";
  argv[n++] = "-Lbuild/winlibs";
  argv[n++] = "-liphlpapi";
  argv[n] = NULL;
  return true;
}

bool Platform_AtomicRename(const char *old_path, const char *new_path) {
  return MoveFileExA(old_path, new_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

int64_t Platform_TimerNowTicks(void) {
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (int64_t)c.QuadPart;
}

int64_t Platform_TimerTickFrequency(void) {
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  return (int64_t)f.QuadPart;
}

struct PlDirIter {
  HANDLE hFind;
  WIN32_FIND_DATAA fd;
  bool first_pending;
};

PlDirIter *Platform_DirOpen(const char *dir_path) {
  if (!dir_path)
    return NULL;
  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(search_path, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return NULL;
  PlDirIter *it = calloc(1, sizeof(*it));
  if (!it) {
    FindClose(h);
    return NULL;
  }
  it->hFind = h;
  it->fd = fd;
  it->first_pending = true;
  return it;
}

bool Platform_DirNext(PlDirIter *it, char *name_out, size_t name_cap,
                      bool *is_dir_out) {
  if (!it || !name_out || name_cap == 0 || !is_dir_out)
    return false;
  for (;;) {
    if (!it->first_pending) {
      if (!FindNextFileA(it->hFind, &it->fd))
        return false;
    }
    it->first_pending = false;
    if (strcmp(it->fd.cFileName, ".") == 0 ||
        strcmp(it->fd.cFileName, "..") == 0)
      continue;
    snprintf(name_out, name_cap, "%s", it->fd.cFileName);
    *is_dir_out = (it->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
  }
}

void Platform_DirClose(PlDirIter *it) {
  if (!it)
    return;
  if (it->hFind != INVALID_HANDLE_VALUE)
    FindClose(it->hFind);
  free(it);
}

float Platform_GetUIScale(void) { return 1.0f; }

void Platform_ApplyUIScale(void) {}

int Platform_GetUnscaledFontSize(void) { return FontSize; }

void Platform_ResetUIScale(void) {}
