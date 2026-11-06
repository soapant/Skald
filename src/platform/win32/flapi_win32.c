#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <netioapi.h>
#include <netlistmgr.h>
#include <initguid.h>
#include "platform/win32/win32_state.h"
#include <ctype.h>
#include "deps/log.h"
#include <shlwapi.h>
#include "core/clipboard_history.h"
#include "platform/flapi_platform.h"
#include "platform/platform.h"
#include <mmdeviceapi.h>
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/modal.h"
#include "core/context.h"
#include "config.h"
#include "core/listbox_hover.h"
#include "core/listbox_layout.h"
#include "core/editbox.h"
#include "core/prompt_geometry.h"
#include "platform/win32/common_ui_win32.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "core/timer.h"

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

bool fl_launch_with_args_platform(const char *path, const char *args) {
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
    log_warn("fl_launch host failed host='%s' params='%s' err=%lu", host,
             hostParams, GetLastError());
  }
  if (win32_exec_shell(path, params[0] ? params : NULL, NULL, "open"))
    return true;
  log_warn("fl_launch ShellExecuteExA failed path='%s' args='%s' err=%lu", path,
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
  log_warn("fl_launch CreateProcessA failed cmd='%s' err=%lu", cmd,
           GetLastError());
  return false;
}

bool fl_launch_with_args_as_admin_platform(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  extern bool Platform_ExpandPath(const char *t, char *o, size_t s);
  if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))
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
    log_warn("fl_launch_admin failed path='%s' err=%lu", path, GetLastError());
  return ok;
}

#include "platform/draw.h"
#include "platform/ui.h"
#include "config.h"
#include "core/window_manager.h"

static PlListSpec g_fl_list_spec;
static ListBoxInfo *g_fl_list_lb;
static int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible, g_fl_list_cur;
static int g_fl_list_bx, g_fl_list_bw, g_fl_list_fw, g_fl_list_h;
static HDC g_fl_list_hdc;
static HFONT g_fl_list_font;
static bool g_fl_list_font_owned;

bool fl_listbox_begin_platform(const PlListSpec *spec) {
  if (!spec)
    return false;
  ListBoxInfo *lb = spec->lb;
  if (!lb) {
    if (!spec->items || spec->count <= 0)
      return false;
    return false;
  }
  Platform_GetDrawContextWin32(&g_fl_list_hdc);
  if (!g_fl_list_hdc)
    return false;
  g_fl_list_spec = *spec;
  g_fl_list_lb = lb;
  PromptState *st = NULL;
  WindowManager *_wm_lb = WindowManager_Get();
  if (_wm_lb) {
    for (int _i = _wm_lb->popup_count - 1; _i >= 0; _i--) {
      Popup *_p = _wm_lb->popups[_i];
      if (_p && _p->user_data) {
        PromptState *_ps = (PromptState *)_p->user_data;
        if (_ps->lb == lb) {
          st = _ps;
          break;
        }
      }
    }
  }
  if (!st) {
    extern PromptState *Platform_GetWin32PromptState(void);
    st = Platform_GetWin32PromptState();
  }
  g_fl_list_itemH = spec->item_height ? spec->item_height : PromptRowHeight();
  ListboxLayout lay;
  Listbox_ComputeLayout(spec, PromptListTop(), g_fl_list_itemH,
                        st ? st->width : 0, st ? st->height : 0,
                        lb->item_count, &lay);
  g_fl_list_y0 = lay.y0;
  g_fl_list_bx = lay.bx;
  g_fl_list_bw = lay.bw;
  g_fl_list_fw = lay.fw;
  g_fl_list_h = lay.list_h;
  g_fl_list_visible = lay.visible;
  UI_LB_SetVisibleCount(lay.visible);
  g_fl_list_cur = 0;
  g_fl_list_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  g_fl_list_font_owned = false;
  ListBox_ApplyPendingScroll(g_fl_list_lb, lay.visible);
  ListBox_SetGeometry(g_fl_list_y0, g_fl_list_itemH, lay.visible);
  ListBox_HoverFocus(g_fl_list_lb, g_fl_list_y0, g_fl_list_itemH,
                     g_fl_list_lb->view_offset, lay.visible);
  return true;
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
  if (!out || !g_fl_list_lb)
    return false;
  if (g_fl_list_cur >= g_fl_list_visible)
    return false;
  int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
  if (idx < 0 || idx >= g_fl_list_lb->item_count)
    return false;
  int y = g_fl_list_y0 + g_fl_list_cur * g_fl_list_itemH;
  if (g_fl_list_cur == 0)
    Platform_DrawPromptSeparator(
        (PlDrawPromptSeparatorSpec){.y = g_fl_list_y0, .width = g_fl_list_fw});
  bool sel = (idx == g_fl_list_lb->selected_index);
  if (RoundedCorners && CornerRadius > 0) {
    int R = PROMPT_SEL_RADIUS;
    if (R > CornerRadius)
      R = CornerRadius;
    int t = PROMPT_SEL_INSET;
    int bx = g_fl_list_bx;
    int bw = g_fl_list_bw;
    RECT inner = {bx + t, y + PROMPT_SEL_TOP_INSET + t, bx + bw - t,
                  y + g_fl_list_itemH - 1 - t};
    int innerR = R - 1;
    if (innerR < 0)
      innerR = 0;
    if (inner.right <= inner.left || inner.bottom <= inner.top) {
      inner = (RECT){bx, y, bx + bw, y + g_fl_list_itemH};
      innerR = R;
    }
    if (sel) {
      win32_fill_rounded_rect(g_fl_list_hdc, &inner, COLOR_SELECTED_TEXT,
                              innerR);
    }
  } else {
    RECT rc = {g_fl_list_bx, y, g_fl_list_bx + g_fl_list_bw,
               y + g_fl_list_itemH};
    if (sel) {
      HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
      FillRect(g_fl_list_hdc, &rc, br);
      DeleteObject(br);
    }
  }
  out->idx = idx;
  out->selected = sel;
  out->x = g_fl_list_spec.x;
  out->y = y;
  out->w = g_fl_list_spec.w;
  out->h = g_fl_list_itemH;
  out->text = g_fl_list_lb->items[idx];
  g_fl_list_cur++;
  return true;
}

void fl_listbox_end_platform(void) {
  if (PromptListBottomMargin() > 0 && g_fl_list_hdc) {
    Platform_DrawPromptSeparator((PlDrawPromptSeparatorSpec){
        .y = g_fl_list_y0 + g_fl_list_h, .width = g_fl_list_fw});
  }
  g_fl_list_lb = NULL;
  g_fl_list_visible = 0;
  g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
  return g_fl_list_lb ? g_fl_list_lb->item_count : 0;
}

unsigned long fl_listbox_get_current_standard_color(void) {

  if (!g_fl_list_lb || g_fl_list_cur == 0)
    return 0;
  int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
  if (idx < 0 || idx >= g_fl_list_lb->item_count)
    return 0;
  int orig_idx = idx;
  int orig_cnt = g_fl_list_lb->item_count;
  if (idx >= 0 && idx < g_fl_list_lb->item_count) {
    orig_idx = g_fl_list_lb->matching_indices[idx];
    orig_cnt = g_fl_list_lb->item_count;
  }
  unsigned long ncol, bcol, pcol;
  Platform_GetItemColors((PlGetItemColorsSpec){.idx = orig_idx,
                                               .count = orig_cnt,
                                               .num_color = &ncol,
                                               .body_color = &bcol,
                                               .plain_color = &pcol});
  return rgb_to_hex(pcol);
}

char *fl_search_file_in_dir_platform(const char *directory,
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
bool fl_type_text_platform(const char *s) {
  if (!s)
    return false;
  char *old = Platform_GetClipboardEntry();
  Platform_CopyToClipboard(s);
  Platform_EmitKey(VK_LCONTROL, 1);
  Platform_EmitKey('V', 1);
  Platform_EmitKey('V', 0);
  Platform_EmitKey(VK_LCONTROL, 0);
  Sleep(100);
  if (old) {
    Platform_CopyToClipboard(old);
    free(old);
  }
  return true;
}

bool fl_place_editbox_platform(const FlEditSpec *spec) {
  if (!spec)
    return false;
  HDC hdc = NULL;
  Platform_GetDrawContextWin32(&hdc);
  if (!hdc)
    return false;
  extern PromptState *Platform_GetWin32PromptState(void);
  PromptState *state = Platform_GetWin32PromptState();
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
                    : FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
  if (hfont && !spec->font.hfont)
    hfont_created = hfont;
  HFONT old = NULL;
  if (hfont)
    old = (HFONT)SelectObject(hdc, hfont);
  SetBkMode(hdc, TRANSPARENT);
  int text_len = (int)strlen(buf);
  int cursor = state ? state->eb->cursor : text_len;
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
  ebi.text_color = rgb_to_hex(COLOR_TEXT);
  ebi.font = spec->font;

  EditboxSpec eb;
  Editbox_BuildSpec(&eb, &ebi);

  Editbox_Render(&eb, NULL);
  if (state)
    state->eb->scroll_offset = eb.scroll;

  if (old)
    SelectObject(hdc, old);
  if (hfont_created)
    DeleteObject(hfont_created);
  return true;
}

typedef struct {
  void (*callback)(void);
} TimerPayload;

VOID CALLBACK TimerTrampoline(PVOID lpParam, BOOLEAN TimerOrWaitFired) {

  TimerPayload *payload = (TimerPayload *)lpParam;
  if (payload) {
    if (payload->callback) {
      payload->callback();
    }
    free(payload);
  }
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
  if (!callback || seconds < 0) {
    return;
  }

  TimerPayload *payload = (TimerPayload *)malloc(sizeof(TimerPayload));
  if (!payload) {
    return;
  }
  payload->callback = callback;

  DWORD delay_ms = (DWORD)(seconds * 1000.0);
  HANDLE hTimer = NULL;
  if (!CreateTimerQueueTimer(&hTimer, NULL, TimerTrampoline, payload, delay_ms,
                             0, WT_EXECUTEONLYONCE)) {
    free(payload);
    return;
  }
}
void fl_sleep_platform(double seconds) {
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
#define FL_NET_ADAPTER_TTL_MS 5000

typedef struct {
  ULONG index;   /* MIB interface index */
  ULONG if_type; /* IF_TYPE_* */
  char guid[64]; /* adapter GUID, as GetIfTable reports it */
  DWORD at_ms;
  bool valid;
} FlNetAdapter;

static CRITICAL_SECTION g_net_cs;
static BOOL g_net_cs_ready = FALSE;
static FlNetStatus g_net_cached = FL_NET_OFFLINE;
static DWORD g_net_cached_at = 0;
static DWORD g_net_ever = 0;
static FlNetAdapter g_net_adapter = {0};

#define FL_NET_TTL_MS 5000

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

static bool net_resolve_adapter(FlNetAdapter *out) {

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

static FlNetAdapter net_active_adapter(void) {

  net_lock();
  DWORD now = GetTickCount();
  if (g_net_adapter.valid &&
      (now - g_net_adapter.at_ms) < FL_NET_ADAPTER_TTL_MS) {
    FlNetAdapter cached = g_net_adapter;
    net_unlock();
    return cached;
  }
  net_unlock();
  FlNetAdapter a;
  memset(&a, 0, sizeof(a));
  a.valid = net_resolve_adapter(&a);
  a.at_ms = now;
  net_lock();
  g_net_adapter = a;
  FlNetAdapter result = g_net_adapter;
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

  g_net_cached = FL_NET_OFFLINE;
  FlNetAdapter a = net_active_adapter();
  if (a.valid) {
    if (a.if_type == IF_TYPE_IEEE80211)
      g_net_cached = FL_NET_WIFI;
    else if (a.if_type == IF_TYPE_ETHERNET_CSMACD)
      g_net_cached = FL_NET_ETHERNET;
  }
}

int fl_get_active_interface_platform(char out[], size_t max_len) {
  if (!out || max_len == 0)
    return 0;
  FlNetAdapter a = net_active_adapter();
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

unsigned long long fl_net_rx_bytes_platform() {

  FlNetAdapter a = net_active_adapter();
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

FlNetStatus fl_net_status_platform(void) {
  DWORD now = GetTickCount();
  net_lock();
  bool fresh = g_net_ever && (now - g_net_cached_at) < FL_NET_TTL_MS;
  if (!fresh) {
    net_unlock();
    net_refresh();
    net_lock();
    g_net_cached_at = GetTickCount();
    g_net_ever = 1;
  }
  FlNetStatus st = g_net_cached;
  net_unlock();
  return st;
}

bool fl_dismiss_message_platform(const char *name) {
  if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) == NULL) {
    return false;
  }
  return Platform_DismissMessage(name);
}

#define FL_HTTP_CONNECT_TIMEOUT_S 10
#define FL_HTTP_TIMEOUT_S 30
#define FL_HTTP_MAX_REDIRECTS 10
#define FL_HTTP_DEFAULT_LIMIT ((size_t)64 * 1024 * 1024)

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
  size_t limit;
  int overflow;
} FlHttpBuf;

static int fl_http_buf_append(FlHttpBuf *b, const char *data, size_t n) {
  if (b->limit && b->len + n > b->limit) {
    size_t room = b->limit > b->len ? b->limit - b->len : 0;
    if (room > 0) {
      memcpy(b->buf + b->len, data, room);
      b->len += room;
      b->buf[b->len] = '\0';
    }
    b->overflow = 1;
    return 0;
  }
  size_t need = b->len + n + 1;
  if (need > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < need)
      cap *= 2;
    char *grown = (char *)realloc(b->buf, cap);
    if (!grown)
      return 0;
    b->buf = grown;
    b->cap = cap;
  }
  memcpy(b->buf + b->len, data, n);
  b->len += n;
  b->buf[b->len] = '\0';
  return 1;
}

static wchar_t *fl_http_widen(const char *s) {
  if (!s || !s[0])
    return NULL;
  int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  if (n <= 0)
    return NULL;
  wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
  if (!w)
    return NULL;
  MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
  return w;
}

static void fl_http_url_parts(const char *url, wchar_t **out_host,
                              wchar_t **out_path, int *out_secure) {
  *out_host = NULL;
  *out_path = NULL;
  *out_secure = 0;
  const char *p = url;
  if (Platform_StringStartsWithCaseInsensitive(p, "https://")) {
    *out_secure = 1;
    p += 8;
  } else if (Platform_StringStartsWithCaseInsensitive(p, "http://")) {
    p += 7;
  } else
    return;
  const char *slash = strchr(p, '/');
  char hostbuf[256];
  if (slash) {
    size_t hl = (size_t)(slash - p);
    if (hl >= sizeof(hostbuf))
      hl = sizeof(hostbuf) - 1;
    memcpy(hostbuf, p, hl);
    hostbuf[hl] = '\0';
    *out_path = fl_http_widen(slash);
  } else {
    snprintf(hostbuf, sizeof(hostbuf), "%s", p);
    *out_path = fl_http_widen("/");
  }
  *out_host = fl_http_widen(hostbuf);
}

static HINTERNET fl_http_session(void) {
  static HINTERNET cached = NULL;
  if (!cached)
    cached = WinHttpOpen(L"Flint/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  return cached;
}

static int fl_http_perform(const char *url, const char *method,
                           const char *content_type, const char *body,
                           FlHttpBuf *buf, FILE *out_file, int redirects_left) {
  wchar_t *host = NULL, *path = NULL;
  int secure = 0;
  fl_http_url_parts(url, &host, &path, &secure);
  if (!host || !path) {
    free(host);
    free(path);
    return 0;
  }
  HINTERNET hs = fl_http_session();
  if (!hs) {
    free(host);
    free(path);
    return 0;
  }
  HINTERNET hc = WinHttpConnect(
      hs, host,
      secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
  if (!hc) {
    free(host);
    free(path);
    return 0;
  }
  wchar_t wmethod[16];
  MultiByteToWideChar(CP_UTF8, 0, method, -1, wmethod, 16);
  HINTERNET hr = WinHttpOpenRequest(hc, wmethod, path, NULL, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    secure ? WINHTTP_FLAG_SECURE : 0);
  free(host);
  free(path);
  if (!hr) {
    WinHttpCloseHandle(hc);
    return 0;
  }
  DWORD t = FL_HTTP_TIMEOUT_S * 1000;
  WinHttpSetTimeouts(hr, (DWORD)(FL_HTTP_CONNECT_TIMEOUT_S * 1000), t, t, t);
  int ok = 0;
  if (body && body[0]) {
    size_t blen = strlen(body);
    wchar_t *wct =
        content_type && content_type[0] ? fl_http_widen(content_type) : NULL;
    ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body,
                            (DWORD)blen, (DWORD)blen, 0)
             ? 1
             : 0;
    if (ok && wct) {
      wchar_t hdr[256];
      _snwprintf(hdr, 256, L"Content-Type: %s", wct);
      hdr[255] = 0;
      ok =
          WinHttpAddRequestHeaders(hr, hdr, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD)
              ? 1
              : 0;
    }
    free(wct);
    if (ok)
      ok = WinHttpReceiveResponse(hr, NULL) ? 1 : 0;
  } else {
    ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
             ? 1
             : 0;
    if (ok)
      ok = WinHttpReceiveResponse(hr, NULL) ? 1 : 0;
  }
  if (ok) {
    DWORD status = 0, sz = sizeof(status);
    WinHttpQueryHeaders(
        hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    if ((status >= 300 && status < 400) && redirects_left > 0) {
      wchar_t *loc = NULL;
      DWORD locsz = 0;
      if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_LOCATION,
                              WINHTTP_HEADER_NAME_BY_INDEX, NULL, &locsz,
                              WINHTTP_NO_HEADER_INDEX) &&
          locsz > 0) {
        loc = (wchar_t *)malloc(locsz);
        if (loc) {
          WinHttpQueryHeaders(hr, WINHTTP_QUERY_LOCATION,
                              WINHTTP_HEADER_NAME_BY_INDEX, loc, &locsz,
                              WINHTTP_NO_HEADER_INDEX);
          char narrow[2048];
          WideCharToMultiByte(CP_UTF8, 0, loc, -1, narrow, sizeof(narrow), NULL,
                              NULL);
          WinHttpCloseHandle(hr);
          WinHttpCloseHandle(hc);
          free(loc);
          int r = fl_http_perform(narrow, method, content_type, body, buf,
                                  out_file, redirects_left - 1);
          return r;
        }
      }
      ok = 1;
    } else {
      DWORD avail = 0;
      while (ok && WinHttpQueryDataAvailable(hr, &avail) && avail > 0) {
        DWORD want = avail > 65536 ? 65536 : avail;
        char *chunk = (char *)malloc(want);
        if (!chunk) {
          ok = 0;
          break;
        }
        DWORD got = 0;
        if (!WinHttpReadData(hr, chunk, want, &got) || got == 0) {
          free(chunk);
          break;
        }
        if (out_file) {
          fwrite(chunk, 1, got, out_file);
        } else if (!fl_http_buf_append(buf, chunk, got)) {
          free(chunk);
          ok = 0;
          break;
        }
        free(chunk);
      }
    }
  }
  WinHttpCloseHandle(hr);
  WinHttpCloseHandle(hc);
  return ok;
}

int fl_get_pid_platform() { return (int)GetCurrentProcessId(); }
