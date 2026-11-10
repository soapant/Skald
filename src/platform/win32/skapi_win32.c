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
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "platform/skapi_platform.h"
#include "platform/platform.h"
#include <mmdeviceapi.h>
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_ui.h"
#include "core/editbox.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "platform/win32/common_ui_win32.h"
#include "helpers/helpers_ui.h"
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

#include "helpers/helpers_listbox_backend.h"

static __thread HDC g_b_hdc = NULL;

bool ListboxBackend_Begin(int item_height_override, ListboxBackendInit *out) {
  (void)item_height_override;
  Platform_GetDrawContextWin32(&g_b_hdc);
  if (!g_b_hdc)
    return false;
  out->item_h = PromptRowHeight();
  out->list_top = PromptListTop();
  PlDrawState_Get()->win32_listbox_surface = true;
  return true;
}

void ListboxBackend_DrawSelection(int bx, int bw, int y, int item_h, int fw,
                                  int frame_h, bool sel) {
  (void)frame_h;
  (void)fw;
  if (RoundedCorners && CornerRadius > 0) {
    int R = PROMPT_SEL_RADIUS;
    if (R > CornerRadius)
      R = CornerRadius;
    int t = PROMPT_SEL_INSET;
    RECT inner = {bx + t, y + PROMPT_SEL_TOP_INSET + t, bx + bw - t,
                  y + item_h - 1 - t};
    int innerR = R - 1;
    if (innerR < 0)
      innerR = 0;
    if (inner.right <= inner.left || inner.bottom <= inner.top) {
      inner = (RECT){bx, y, bx + bw, y + item_h};
      innerR = R;
    }
    if (sel)
      win32_fill_rounded_rect(g_b_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
  } else if (sel) {
    RECT rc = {bx, y, bx + bw, y + item_h};
    HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
    FillRect(g_b_hdc, &rc, br);
    DeleteObject(br);
  }
}

void ListboxBackend_End(void) {
  PlDrawState_Get()->win32_listbox_surface = false;
  g_b_hdc = NULL;
}

PromptState *ListboxBackend_PromptState(const ListBoxInfo *lb) {
  WindowManager *wm = WindowManager_Get();
  if (wm) {
    for (int i = wm->popup_count - 1; i >= 0; i--) {
      Popup *p = wm->popups[i];
      if (p && p->user_data) {
        PromptState *ps = (PromptState *)p->user_data;
        if (ps->lb == lb)
          return ps;
      }
    }
  }
  {
    extern PromptState *Platform_GetWin32PromptState(void);
    return Platform_GetWin32PromptState();
  }
}

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string,
                                     char *output_path) {

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
    if (Platform_StringContainsCaseInsensitive(find_data.cFileName,
                                               search_string)) {
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
  Platform_GetDrawContextWin32(&hdc);
  if (!hdc)
    return false;
  extern PromptState *Platform_GetWin32PromptState(void);
  PromptState *state = Platform_GetWin32PromptState();
  int fs = spec->fontsize > 0 ? spec->fontsize : FontSize;
  const char *buf = spec->buffer;
  if (!buf) {
    if (state)
      buf = state->eb->filter;
    else
      buf = "";
  }
  int x = spec->x;
  int y = spec->y;
  int w = spec->w;
  int h = spec->h;
  if (w <= 0)
    w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
  if (h <= 0)
    h = PromptCustomEditHeight();
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
  int text_len = (int)strlen(buf);
  int cursor = state ? (int)state->eb->cursor : text_len;
  if (cursor < 0)
    cursor = 0;
  if (cursor > text_len)
    cursor = text_len;
  int cursor_x = 0;
  int text_w = 0;
  int y_raw = (spec->y == 0) ? PROMPT_EDIT_INSET : spec->y;
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
  memset(&ebi, 0, sizeof(ebi));
  ebi.state = state;
  ebi.text = buf;
  ebi.text_len = text_len;
  ebi.cursor = cursor;
  ebi.x = x;
  ebi.y = y_raw;
  ebi.w = w;
  ebi.h = h;
  ebi.cursor_x = cursor_x;
  ebi.text_w = text_w;
  ebi.ascent = ascent;
  ebi.descent = descent;
  ebi.text_color =
      spec->color_text ? rgb_to_hex(spec->color_text) : rgb_to_hex(COLOR_TEXT);
  ebi.bg_color =
      spec->color_bg ? rgb_to_hex(spec->color_bg) : rgb_to_hex(COLOR_BG);
  ebi.placeholder = spec->placeholder;
  ebi.caret_height = spec->caret_height;
  ebi.caret_width = spec->caret_width;
  ebi.caret_x = spec->caret_x;
  ebi.caret_y = spec->caret_y;
  ebi.caret_pos = spec->caret_pos;
  ebi.fontsize = fs;
  ebi.font = spec->font;

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

static VOID CALLBACK TimerTrampoline(PVOID lpParam,
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
  if (!CreateTimerQueueTimer(&hTimer, NULL, TimerTrampoline, payload, delay_ms,
                             0, WT_EXECUTEONLYONCE)) {
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

/* GetAdaptersAddresses costs ~6ms on a machine with many adapters (Hyper-V,
   WSL, VPN, Bluetooth, ...), and the net widgets used to call it on every
   sample. The active adapter changes rarely, so resolve it once and reuse
   the interface index for a few seconds. GetIfEntry on that index is then a
   ~0.08ms single-interface query instead of two full GetIfTable sweeps. */
#define SK_NET_ADAPTER_TTL_MS 5000

typedef struct {
  ULONG index;   /* MIB interface index */
  ULONG if_type; /* IF_TYPE_* */
  char guid[64]; /* adapter GUID, as GetIfTable reports it */
  DWORD at_ms;
  bool valid;
} SkNetAdapter;

static CRITICAL_SECTION g_net_cs;
static BOOL g_net_cs_ready = FALSE;
static SkNetStatus g_net_cached = SK_NET_OFFLINE;
static DWORD g_net_cached_at = 0;
static DWORD g_net_ever = 0;
static SkNetAdapter g_net_adapter = {0};

#define SK_NET_TTL_MS 5000

static void net_lock(void) {

  if (!g_net_cs_ready) {
    InitializeCriticalSection(&g_net_cs);
    g_net_cs_ready = TRUE;
  }
  EnterCriticalSection(&g_net_cs);
}

static void net_unlock(void) {

  if (g_net_cs_ready)
    LeaveCriticalSection(&g_net_cs);
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
    strncpy(out->guid, id, sizeof(out->guid) - 1);
    out->guid[sizeof(out->guid) - 1] = '\0';
    found = true;
    break;
  }
  free(addrs);
  return found;
}

static SkNetAdapter net_active_adapter(void) {

  net_lock();
  DWORD now = GetTickCount();
  if (g_net_adapter.valid &&
      (now - g_net_adapter.at_ms) < SK_NET_ADAPTER_TTL_MS) {
    SkNetAdapter cached = g_net_adapter;
    net_unlock();
    return cached;
  }
  net_unlock();
  SkNetAdapter a;
  memset(&a, 0, sizeof(a));
  a.valid = net_resolve_adapter(&a);
  a.at_ms = now;
  net_lock();
  g_net_adapter = a;
  SkNetAdapter result = g_net_adapter;
  net_unlock();
  return result;
}

static void net_invalidate_adapter(void) {
  net_lock();
  g_net_adapter.valid = false;
  g_net_adapter.index = 0;
  net_unlock();
}

static void net_refresh(void) {

  g_net_cached = SK_NET_OFFLINE;
  SkNetAdapter a = net_active_adapter();
  if (a.valid) {
    if (a.if_type == IF_TYPE_IEEE80211)
      g_net_cached = SK_NET_WIFI;
    else if (a.if_type == IF_TYPE_ETHERNET_CSMACD)
      g_net_cached = SK_NET_ETHERNET;
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

unsigned long long sk_net_rx_bytes_platform() {

  SkNetAdapter a = net_active_adapter();
  if (!a.valid || !a.index)
    return 0;
  MIB_IFROW row;
  memset(&row, 0, sizeof(row));
  row.dwIndex = a.index;
  if (GetIfEntry(&row) != NO_ERROR) {
    /* The interface went away or the index was recycled: resolve again. */
    net_invalidate_adapter();
    return 0;
  }
  unsigned long long rx = (unsigned long long)row.dwInOctets;
  return rx;
}

SkNetStatus sk_net_status_platform(void) {
  DWORD now = GetTickCount();
  net_lock();
  bool fresh = g_net_ever && (now - g_net_cached_at) < SK_NET_TTL_MS;
  if (!fresh) {
    net_unlock();
    net_refresh();
    net_lock();
    g_net_cached_at = GetTickCount();
    g_net_ever = 1;
  }
  SkNetStatus st = g_net_cached;
  net_unlock();
  return st;
}

bool sk_dismiss_message_platform(const char *name) {
  if (!MessagePopup_AnyOpen()) {
    return false;
  }
  return Platform_DismissMessage(name);
}

int sk_get_pid_platform() { return (int)GetCurrentProcessId(); }
