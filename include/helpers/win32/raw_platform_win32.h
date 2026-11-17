#ifndef RAW_PLATFORM_WIN32_H
#define RAW_PLATFORM_WIN32_H

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/types/window.h"
#include "core/types.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_fs.h"
#include <winhttp.h>
#include <psapi.h>
#include <dwmapi.h>
#include "config_values.h"
#include "core/types/keys.h"
#include "deps/log.h"
#include <fileapi.h>
#include <handleapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/context.h"
#include "core/feature_path.h"
#include "platform/platform.h"
#include "helpers/win32/win32_state.h"
#include "helpers/win32/subclasses_win32.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/bg_image_win32.h"
#include "core/prompt_geometry.h"
#include "helpers/win32/raw_input_core_win32.h"
#include <shellapi.h>
#include <gdiplus.h>
#include <ctype.h>
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_owner.h"
#include "core/thread_registry.h"

typedef struct {
  HANDLE pipe_write;
  const char *data;
  size_t len;
} StdinWriteCtx;

typedef struct {
  const char *target_title;
  HWND found_hwnd;
} FindWindowData;

typedef struct {
  void *(*fn)(void *);
  void *arg;
} WinThreadStart;

typedef struct {
  RECT rc;
  char name[128];
  bool is_primary;
} Win32MonitorEntry;

typedef struct {
  Win32MonitorEntry list[16];
  int count;
} Win32MonitorList;

extern HANDLE g_singleton_mtx;
extern HHOOK s_win32_keyboard_hook;
extern HHOOK s_win32_mouse_hook;
extern uint32_t s_main_thread_id;
extern WindowHandle s_tray_wnd;

#define WIN32_FOCUS_LEFT 0
#define WIN32_FOCUS_RIGHT 1
#define WIN32_FOCUS_UP 2
#define WIN32_FOCUS_DOWN 3

#define WIN32_MON_LEFT 0
#define WIN32_MON_RIGHT 1
#define WIN32_MON_UP 2
#define WIN32_MON_DOWN 3

extern BOOL g_win32_fs_active;
extern char g_win32_project_root[MAX_PATH];
extern HWND g_win32_last_min;
extern RECT g_win32_fs_rect;
extern LONG g_win32_fs_style;
extern LONG g_win32_fs_exstyle;
extern ULONG_PTR g_gdiplusToken;

DWORD WINAPI win32_stdin_writer(LPVOID arg);
bool win32_cmdline_append_arg(char *cmd, size_t cap, size_t *off,
                              const char *arg);
BOOL CALLBACK win32_enum_windows_proc_title(HWND hwnd, LPARAM lParam);
HWND win32_foreground_target(void);
BOOL CALLBACK win32_mon_enum(HMONITOR hMon, HDC hdc, LPRECT rc, LPARAM lParam);
BOOL CALLBACK win32_focus_enum(HWND hwnd, LPARAM lp);
bool win32_move_active_window_direction(int dir);
bool win32_activate_hwnd(HWND h);
bool win32_focus_direction(int dir);
bool win32_placeholder_icon(char *out, size_t out_len);
bool win32_is_shell_icon(const char *p);
unsigned win32_icon_hash(const char *s);
bool win32_icon_cache_dir(char *out, size_t n);
bool win32_icon_file_ok(const char *path);
bool win32_icon_png_path(const char *src, char *out, size_t n);
void win32_gdiplus_ready(void);
bool win32_export_shell_icon_png(const char *native_path);
BOOL CALLBACK win32_enum_windows_proc_pair(HWND hwnd, LPARAM lParam);
void win32_collect_pair(WindowList *out_titles, WindowList *out_names);
int win32_dir_exists(const char *path);
int win32_is_project_root(const char *dir);
DWORD win32_button_flag(int button, bool down);

#endif
