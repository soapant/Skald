#include "helpers/helpers_text.h"
#include <winsock2.h>
#include "helpers/helpers_fs.h"
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <netlistmgr.h>
#include <initguid.h>
#include "core/types.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/types/skapi_types.h"
#include "config_values.h"
#include "helpers/win32/win32_state.h"
#include "deps/log.h"
#include "platform/platform.h"
#include <mmdeviceapi.h>
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "core/editbox.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "helpers/win32/common_ui_win32.h"
#include "core/thread_registry.h"
#include "helpers/helpers_owner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *win32_host_for_system_file(const char *path) {

  const char *dot = strrchr(path, '.');
  if (!dot)
    return NULL;
  if (_stricmp(dot, ".cpl") == 0)
    return "control.exe";
  if (_stricmp(dot, ".msc") == 0)
    return "mmc.exe";
  return NULL;
}

static bool win32_exec_shell(const char *file, const char *params,
                             const char *working_dir, const char *verb) {

  SHELLEXECUTEINFOA sei;
  ZeroMemory(&sei, sizeof(sei));
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  sei.lpVerb = verb;
  sei.lpFile = file;
  sei.lpParameters = (params && params[0]) ? params : NULL;
  sei.lpDirectory = working_dir;
  sei.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExA(&sei))
    return false;
  if (sei.hProcess)
    CloseHandle(sei.hProcess);
  return true;
}

bool sk_launch_with_args_platform(const char *path, const char *args) {
  if (!path)
    return false;
  char params[2048];
  params[0] = '\0';
  if (args && args[0] && strcmp(args, "0") != 0) {
    snprintf(params, sizeof(params), "%s", args);
  }
  const char *host = win32_host_for_system_file(path);
  if (host) {
    char hostParams[2048];
    hostParams[0] = '\0';
    if (params[0])
      snprintf(hostParams, sizeof(hostParams), "\"%s\" %s", path, params);
    else
      snprintf(hostParams, sizeof(hostParams), "\"%s\"", path);
    if (win32_exec_shell(host, hostParams, NULL, "open"))
      return true;
    log_warn("sk_launch host failed host='%s' params='%s' err=%lu", host,
             hostParams, GetLastError());
  }
  if (win32_exec_shell(path, params[0] ? params : NULL, NULL, "open"))
    return true;
  log_warn("sk_launch ShellExecuteExA failed path='%s' args='%s' err=%lu", path,
           params, GetLastError());
  char cmd[2048];
  if (params[0])
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, params);
  else
    snprintf(cmd, sizeof(cmd), "\"%s\"", path);
  STARTUPINFOA si = {0};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {0};
  if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                     &si, &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
  }
  log_warn("sk_launch CreateProcessA failed cmd='%s' err=%lu", cmd,
           GetLastError());
  return false;
}

bool sk_launch_with_args_as_admin_platform(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  path = expanded;
  char params[2048] = {0};
  const char *host = win32_host_for_system_file(path);
  if (host) {
    if (args && args[0])
      snprintf(params, sizeof(params), "\"%s\" %s", path, args);
    else
      snprintf(params, sizeof(params), "\"%s\"", path);
  } else if (args && args[0])
    snprintf(params, sizeof(params), "%s", args);
  SHELLEXECUTEINFOA sei = {0};
  sei.cbSize = sizeof(sei);
  sei.lpVerb = "runas";
  sei.lpFile = host ? host : path;
  sei.lpParameters = params[0] ? params : NULL;
  sei.nShow = SW_SHOWNORMAL;
  BOOL ok = ShellExecuteExA(&sei);
  if (!ok)
    log_warn("sk_launch_admin failed path='%s' err=%lu", path, GetLastError());
  return ok;
}

#include "core/window_manager.h"

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string, ) {

  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*", directory);

  WIN32_FIND_DATAA find_data;
  HANDLE hFind = FindFirstFileA(search_path, &find_data);

  if (hFind == INVALID_HANDLE_VALUE) {
    return NULL;
  }

  static char found_path[MAX_PATH];

  do {
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    if (String_ContainsCI(find_data.cFileName, search_string)) {
      snprintf(found_path, sizeof(found_path), "%s\\%s", directory,
               find_data.cFileName);
      FindClose(hFind);
      return found_path;
    }
  } while (FindNextFileA(hFind, &find_data));

  FindClose(hFind);
  return NULL;
}

bool sk_place_editbox_platform(const SkEditSpec *spec) {
  if (!spec)
    return false;
  HDC hdc = NULL;
  win32_get_draw_context(&hdc);
  if (!hdc)
    return false;
  PromptState *state = PromptPopup_GetTopPromptState();
  EditboxPlace place;
  Editbox_ResolvePlace(spec->buffer, spec->x, spec->y, spec->w, spec->h,
                       spec->fontsize, state, &place);
  int fs = place.fontsize;
  const char *buf = place.buf;
  int x = place.x;
  int y = place.y;
  int w = place.w;
  int h = place.h;
  int text_len = place.text_len;
  int cursor = place.cursor;
  int y_raw = place.y_raw;
  HFONT hfont_created = NULL;
  HFONT hfont = spec->font.hfont
                    ? spec->font.hfont
                    : FH_TO_HFONT(Platform_CreateFont(fs, UsedFont));
  if (hfont && !spec->font.hfont)
    hfont_created = hfont;
  HFONT old = NULL;
  if (hfont)
    old = (HFONT)SelectObject(hdc, hfont);
  SetBkMode(hdc, TRANSPARENT);
  int cursor_x = 0;
  int text_w = 0;
  int ascent = 0;
  int descent = 0;
  if (hfont) {
    TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    GetTextMetrics(hdc, &tm);
    ascent = tm.tmAscent;
    descent = tm.tmDescent;
    SIZE szCur;
    GetTextExtentPoint32A(hdc, buf, cursor, &szCur);
    cursor_x = szCur.cx;
    SIZE szText;
    GetTextExtentPoint32A(hdc, buf, text_len, &szText);
    text_w = szText.cx;
  } else {
    RECT textRc = {x + PROMPT_EDIT_INSET, y, x + w, y + h};
    DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
  }
  EditboxInit ebi;
  EditboxInitFromSpec init_args = {.state = state,
                                   .buf = buf,
                                   .text_len = text_len,
                                   .cursor = cursor,
                                   .x = x,
                                   .y = y_raw,
                                   .w = w,
                                   .h = h,
                                   .cursor_x = cursor_x,
                                   .text_w = text_w,
                                   .ascent = ascent,
                                   .descent = descent,
                                   .fontsize = fs,
                                   .font = spec->font,
                                   .text_color = spec->color_text,
                                   .bg_color = spec->color_bg,
                                   .placeholder = spec->placeholder,
                                   .caret_height = spec->caret_height,
                                   .caret_width = spec->caret_width,
                                   .caret_x = spec->caret_x,
                                   .caret_y = spec->caret_y,
                                   .caret_pos = spec->caret_pos};
  Editbox_InitFromSpec(&ebi, &init_args);

  EditboxSpec eb;
  Editbox_BuildSpec(&eb, &ebi);

  bool prev_lb = PlDrawState_Get()->win32_listbox_surface;
  PlDrawState_Get()->win32_listbox_surface = false;
  Editbox_Render(&eb, NULL);
  PlDrawState_Get()->win32_listbox_surface = prev_lb;
  if (state)
    state->eb->scroll_offset = eb.scroll;

  if (old)
    SelectObject(hdc, old);
  if (hfont_created)
    DeleteObject(hfont_created);
  return true;
}

static VOID CALLBACK win32_timer_trampoline(PVOID lpParam,
                                            BOOLEAN TimerOrWaitFired) {

  SkRunAfterPayload *payload = (SkRunAfterPayload *)lpParam;
  if (payload) {
    if (payload->callback) {
      payload->callback();
    }
    ThreadRegistry_Finish(payload);
    free(payload);
  }
}

bool sk_run_after_time_arm_platform(SkRunAfterPayload *payload) {

  DWORD delay_ms = (DWORD)(payload->seconds * 1000.0);
  HANDLE hTimer = NULL;
  if (!CreateTimerQueueTimer(&hTimer, NULL, win32_timer_trampoline, payload,
                             delay_ms, 0, WT_EXECUTEONLYONCE)) {
    return false;
  }
  return true;
}

void sk_sleep_platform(double seconds) {
  if (seconds <= 0) {
    return;
  }
  double ms = seconds * 1000.0;
  if (ms > (double)0xFFFFFFFF)
    ms = (double)0xFFFFFFFF;
  Sleep((DWORD)ms);
}

#define SK_NET_ADAPTER_TTL_MS 5000

typedef struct {
  ULONG index;
  ULONG if_type;
  char guid[64];
  DWORD at_ms;
  bool valid;
} SkNetAdapter;

static CRITICAL_SECTION net_cs;
static INIT_ONCE net_cs_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK net_cs_init(PINIT_ONCE once, PVOID param, PVOID *ctx) {
  InitializeCriticalSection(&net_cs);
  return TRUE;
}
static SkNetStatus net_cached = SK_NET_OFFLINE;
static DWORD net_cached_at = 0;
static DWORD net_ever = 0;
static SkNetAdapter net_adapter = {0};

#define SK_NET_TTL_MS 5000

static void net_lock(void) {

  InitOnceExecuteOnce(&net_cs_once, net_cs_init, NULL, NULL);
  EnterCriticalSection(&net_cs);
}

static void net_unlock(void) {

  InitOnceExecuteOnce(&net_cs_once, net_cs_init, NULL, NULL);
  LeaveCriticalSection(&net_cs);
}

static bool net_resolve_adapter(SkNetAdapter *out) {

  ULONG size = 16 * 1024;
  IP_ADAPTER_ADDRESSES *addrs = (IP_ADAPTER_ADDRESSES *)malloc(size);
  if (!addrs)
    return false;
  ULONG rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL,
                                  addrs, &size);
  if (rc == ERROR_BUFFER_OVERFLOW) {
    free(addrs);
    addrs = (IP_ADAPTER_ADDRESSES *)malloc(size);
    if (!addrs)
      return false;
    rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, addrs,
                              &size);
  }
  if (rc != NO_ERROR) {
    free(addrs);
    return false;
  }
  bool found = false;
  for (IP_ADAPTER_ADDRESSES *a = addrs; a; a = a->Next) {
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    if (a->OperStatus != IfOperStatusUp)
      continue;
    if (a->FirstGatewayAddress == NULL)
      continue;
    out->index = a->IfIndex;
    out->if_type = a->IfType;
    const char *id = a->AdapterName ? a->AdapterName : "";
    SafeCopy(out->guid, sizeof(out->guid), id);
    found = true;
    break;
  }
  free(addrs);
  return found;
}

static SkNetAdapter net_active_adapter(void) {

  net_lock();
  DWORD now = GetTickCount();
  if (net_adapter.valid && (now - net_adapter.at_ms) < SK_NET_ADAPTER_TTL_MS) {
    SkNetAdapter cached = net_adapter;
    net_unlock();
    return cached;
  }
  net_unlock();
  SkNetAdapter a;
  memset(&a, 0, sizeof(a));
  a.valid = net_resolve_adapter(&a);
  a.at_ms = now;
  net_lock();
  net_adapter = a;
  SkNetAdapter result = net_adapter;
  net_unlock();
  return result;
}

static void net_invalidate_adapter(void) {
  net_lock();
  net_adapter.valid = false;
  net_adapter.index = 0;
  net_unlock();
}

static void net_refresh(void) {

  net_lock();
  net_cached = SK_NET_OFFLINE;
  net_unlock();
  SkNetAdapter a = net_active_adapter();
  if (a.valid) {
    net_lock();
    if (a.if_type == IF_TYPE_IEEE80211)
      net_cached = SK_NET_WIFI;
    else if (a.if_type == IF_TYPE_ETHERNET_CSMACD)
      net_cached = SK_NET_ETHERNET;
    net_unlock();
  }
}

int sk_get_active_interface_platform(char out[], size_t max_len) {
  if (!out || max_len == 0)
    return 0;
  SkNetAdapter a = net_active_adapter();
  if (!a.valid || !a.guid[0]) {
    out[0] = '\0';
    return 0;
  }
  size_t len = strlen(a.guid);
  if (len >= max_len)
    len = max_len - 1;
  memcpy(out, a.guid, len);
  out[len] = '\0';
  return 1;
}

unsigned long long sk_net_rx_bytes_platform(void) {

  SkNetAdapter a = net_active_adapter();
  if (!a.valid || !a.index)
    return 0;
  MIB_IFROW row;
  memset(&row, 0, sizeof(row));
  row.dwIndex = a.index;
  if (GetIfEntry(&row) != NO_ERROR) {
    net_invalidate_adapter();
    return 0;
  }
  return (unsigned long long)row.dwInOctets;
}

SkNetStatus sk_net_status_platform(void) {
  DWORD now = GetTickCount();
  net_lock();
  bool fresh = net_ever && (now - net_cached_at) < SK_NET_TTL_MS;
  if (!fresh) {
    net_unlock();
    net_refresh();
    net_lock();
    net_cached_at = GetTickCount();
    net_ever = 1;
  }
  SkNetStatus st = net_cached;
  net_unlock();
  return st;
}

bool sk_dismiss_message_platform(const char *name) {
  if (!win32_message_popup_any_open()) {
    return false;
  }
  return Platform_DismissMessage(name);
}

int sk_get_pid_platform(void) { return (int)GetCurrentProcessId(); }
