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
#include "platform/flapi_platform.h"
#include "core/clipboard_history.h"
#include "platform/platform.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/modal.h"
#include "config.h"
#include "core/prompt_geometry.h"
#include "bindings/vim_state.h"
#include "platform/win32/common_ui_win32.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/timer.h"
bool Platform_GetAllFilesFromDir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore, bool strip_extension) {
    PROFILE_START(_pt);

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) { PROFILE_END_VAR(_pt, "Platform_GetAllFilesFromDir"); return false; }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            bool should_ignore = false;
            if (list_of_names_to_ignore) {
                for (int i = 0; list_of_names_to_ignore[i] != NULL; i++) {
                    if (Platform_StringContainsCaseInsensitive(find_data.cFileName, list_of_names_to_ignore[i])) {
                        should_ignore = true;
                        break;
                    }
                }
            }
            if (!should_ignore) {
                Platform_GetAllFilesFromDir(full_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore, strip_extension);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(find_data.cFileName, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        const char* want = list_of_allowed_extensions[i];
                        if (want && want[0] == '.') want++;
                        if (Platform_StringContainsCaseInsensitive(ext, want)) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }

            if (allowed) {
                if (list_of_files->count >= list_of_files->capacity) {
                    list_of_files->capacity = list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;
                    list_of_files->files = realloc(list_of_files->files, list_of_files->capacity * sizeof(FileItem));
                }

                FileItem* item = &list_of_files->files[list_of_files->count];
                strncpy(item->path, full_path, sizeof(item->path) - 1);
                item->path[sizeof(item->path) - 1] = '\0';

                char clean_name[256];
                strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                clean_name[sizeof(clean_name) - 1] = '\0';
                if (strip_extension) {
                    char* dot = strrchr(clean_name, '.');
                    if (dot) *dot = '\0';
                }
                strncpy(item->name, clean_name, sizeof(item->name) - 1);
                item->name[sizeof(item->name) - 1] = '\0';

                list_of_files->count++;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    { PROFILE_END_VAR(_pt, "Platform_GetAllFilesFromDir"); return true; }
}

static const char* win32_host_for_system_file(const char* path) {
        PROFILE_START(_pt);
const char* dot = strrchr(path, '.');
    if (!dot) { PROFILE_END_VAR(_pt, "win32_host_for_system_file"); return NULL; }
    if (_stricmp(dot, ".cpl") == 0) { PROFILE_END_VAR(_pt, "win32_host_for_system_file"); return "control.exe"; }
    if (_stricmp(dot, ".msc") == 0) { PROFILE_END_VAR(_pt, "win32_host_for_system_file"); return "mmc.exe"; }
    { PROFILE_END_VAR(_pt, "win32_host_for_system_file"); return NULL; }
}

static bool win32_exec_shell(const char* file, const char* params, const char* working_dir, const char* verb) {
        PROFILE_START(_pt);
SHELLEXECUTEINFOA sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = verb;
    sei.lpFile = file;
    sei.lpParameters = (params && params[0]) ? params : NULL;
    sei.lpDirectory = working_dir;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&sei)) { PROFILE_END_VAR(_pt, "win32_exec_shell"); return false; }
    if (sei.hProcess) CloseHandle(sei.hProcess);
    { PROFILE_END_VAR(_pt, "win32_exec_shell"); return true; }
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
        PROFILE_START(_pt);
if (!path) { PROFILE_END_VAR(_pt, "fl_launch_with_args_platform"); return false; }
    char params[2048];
    params[0] = '\0';
    if (args && args[0] && strcmp(args, "0") != 0) {
        snprintf(params, sizeof(params), "%s", args);
    }
    const char* host = win32_host_for_system_file(path);
    if (host) {
        char hostParams[2048];
        hostParams[0] = '\0';
        if (params[0]) snprintf(hostParams, sizeof(hostParams), "\"%s\" %s", path, params);
        else snprintf(hostParams, sizeof(hostParams), "\"%s\"", path);
        if (win32_exec_shell(host, hostParams, NULL, "open")) return true;
        log_warn("fl_launch host failed host='%s' params='%s' err=%lu", host, hostParams, GetLastError());
    }
    if (win32_exec_shell(path, params[0] ? params : NULL, NULL, "open")) { PROFILE_END_VAR(_pt, "fl_launch_with_args_platform"); return true; }
    log_warn("fl_launch ShellExecuteExA failed path='%s' args='%s' err=%lu", path, params, GetLastError());
    char cmd[2048];
    if (params[0]) snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, params);
    else snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    STARTUPINFOA si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return true;
    }
    log_warn("fl_launch CreateProcessA failed cmd='%s' err=%lu", cmd, GetLastError());
    { PROFILE_END_VAR(_pt, "fl_launch_with_args_platform"); return false; }
}

bool fl_launch_with_args_as_admin_platform(const char* path, const char* args) {
        PROFILE_START(_pt);
if (!path) { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin_platform"); return false; }
    char expanded[MAX_PATH];
    extern bool Platform_ExpandPath(const char* t, char* o, size_t s);
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin_platform"); return false; }
    path = expanded;
    char params[2048] = {0};
    const char* host = win32_host_for_system_file(path);
    if (host) {
        if (args && args[0]) snprintf(params, sizeof(params), "\"%s\" %s", path, args);
        else snprintf(params, sizeof(params), "\"%s\"", path);
    } else if (args && args[0]) snprintf(params, sizeof(params), "%s", args);
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = host ? host : path;
    sei.lpParameters = params[0] ? params : NULL;
    sei.nShow = SW_SHOWNORMAL;
    BOOL ok = ShellExecuteExA(&sei);
    if (!ok) log_warn("fl_launch_admin failed path='%s' err=%lu", path, GetLastError());
    { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin_platform"); return ok; }
}

#include "platform/draw.h"
#include "platform/ui.h"
#include "config.h"

static FlListSpec g_fl_list_spec;
static ListBoxInfo *g_fl_list_lb;
static int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible, g_fl_list_cur;
static int g_fl_list_bx, g_fl_list_bw, g_fl_list_fw;
static HDC g_fl_list_hdc;
static HFONT g_fl_list_font;
static bool g_fl_list_font_owned;

bool fl_listbox_begin_platform(const FlListSpec *spec) {
        PROFILE_START(_pt);
if (!spec) { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return false; }
    ListBoxInfo *lb = spec->lb;
    if (!lb) {
        if (!spec->items || spec->count <= 0) return false;
        return false;
    }
    if (!lb->populated_count) { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return false; }
    Platform_GetDrawContextWin32(&g_fl_list_hdc);
    if (!g_fl_list_hdc) { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return false; }
    g_fl_list_spec = *spec;
    g_fl_list_lb = lb;
    extern PromptState* Platform_GetWin32PromptState(void);
    PromptState *st = Platform_GetWin32PromptState();
    g_fl_list_itemH = spec->item_height ? spec->item_height : PromptRowHeight();
    g_fl_list_y0 = spec->y ? spec->y : PromptListTop();
    int bottomMargin = PromptListBottomMargin();
    int list_h;
    if (spec->h) list_h = spec->h;
    else list_h = st ? (st->height - g_fl_list_y0 - bottomMargin) : PROMPT_CUSTOM_FALLBACK_LIST_H;
    if (!spec->x && !spec->w) {
        g_fl_list_bx = PROMPT_SEL_X;
        g_fl_list_bw = (st ? st->width : PROMPT_W) - PROMPT_SEL_W_SHRINK;
        g_fl_list_fw = st ? st->width : PROMPT_W;
    } else {
        g_fl_list_bx = spec->x + PROMPT_SEL_INSET;
        g_fl_list_bw = (spec->w ? spec->w : PROMPT_CUSTOM_FALLBACK_LIST_W) - 2;
        g_fl_list_fw = spec->w ? spec->w : PROMPT_W;
    }
    int visible = list_h / g_fl_list_itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;
    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;
    g_fl_list_visible = visible;
    g_fl_list_cur = 0;
    g_fl_list_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_fl_list_font_owned = false;
    { PROFILE_END_VAR(_pt, "fl_listbox_begin_platform"); return true; }
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
        PROFILE_START(_pt);
if (!out || !g_fl_list_lb) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    if (g_fl_list_cur >= g_fl_list_visible) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return false; }
    int y = g_fl_list_y0 + g_fl_list_cur * g_fl_list_itemH;
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (RoundedCorners && CornerRadius>0) {
        int R=PROMPT_SEL_RADIUS; if (R > CornerRadius) R = CornerRadius;
        int t = PROMPT_SEL_INSET;
        int bx = g_fl_list_bx;
        int bw = g_fl_list_bw;
        RECT inner = {bx+t, y+PROMPT_SEL_TOP_INSET+t, bx+bw-t, y+g_fl_list_itemH-1-t};
        int innerR = R-1; if (innerR < 0) innerR = 0;
        if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = (RECT){bx, y, bx+bw, y+g_fl_list_itemH}; innerR = R; }
        if (sel) {
            win32_fill_rounded_rect(g_fl_list_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
        }
    } else {
        RECT rc = { g_fl_list_bx, y, g_fl_list_bx + g_fl_list_bw, y + g_fl_list_itemH };
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
    out->text = g_fl_list_lb->populated_cache[idx];
    g_fl_list_cur++;
    { PROFILE_END_VAR(_pt, "fl_listbox_next_platform"); return true; }
}

void fl_listbox_end_platform(void) {
        PROFILE_START(_pt);
g_fl_list_lb = NULL;
    g_fl_list_visible = 0;
    g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_listbox_get_count"); return g_fl_list_lb ? g_fl_list_lb->populated_count : 0; }
}

unsigned long fl_listbox_get_current_standard_color(void) {
        PROFILE_START(_pt);
if (!g_fl_list_lb || g_fl_list_cur == 0) { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return 0; }
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return 0; }
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (sel) { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return rgb_to_hex(COLOR_TEXT); }
    int orig_idx = idx;
    int orig_cnt = g_fl_list_lb->populated_count;
    if (idx >= 0 && idx < g_fl_list_lb->populated_count) {
        orig_idx = g_fl_list_lb->matching_indices[idx];
        orig_cnt = g_fl_list_lb->last_item_count > 0 ? g_fl_list_lb->last_item_count : g_fl_list_lb->populated_count;
    }
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(orig_idx, orig_cnt, &ncol, &bcol, &pcol);
    { PROFILE_END_VAR(_pt, "fl_listbox_get_current_standard_color"); return rgb_to_hex(pcol); }
}

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
        PROFILE_START(_pt);
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
        if (Platform_StringContainsCaseInsensitive(find_data.cFileName, search_string)) {
            snprintf(found_path, sizeof(found_path), "%s\\%s", directory, find_data.cFileName);
            FindClose(hFind);
            return found_path;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    { PROFILE_END_VAR(_pt, "fl_search_file_in_dir_platform"); return NULL; }
}
#define VK_MEDIA_PLAY_PAUSE 0xB3

void fl_toggle_play_pause_platform(void) {
        PROFILE_START(_pt);
INPUT input[2] = {0};

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, input, sizeof(INPUT));
}

static int vol_query_raw(void) {
        PROFILE_START(_pt);
HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    float currentVolume = 0.0f;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
    } else if (FAILED(hr)) {
        return 0;
    }

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    pVolume->lpVtbl->GetMasterVolumeLevelScalar(pVolume, &currentVolume);

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();

    { PROFILE_END_VAR(_pt, "vol_query_raw"); return (int)(currentVolume * 100.0f + 0.5f); }
}

#define FL_VOL_TTL_MS 500

int fl_get_current_volume_platform(void) {
        PROFILE_START(_pt);
static CRITICAL_SECTION cs;
    static BOOL cs_ready = FALSE;
    static int cached = -1;
    static DWORD cached_at = 0;
    static BOOL ever = FALSE;

    if (!cs_ready) { InitializeCriticalSection(&cs); cs_ready = TRUE; }
    EnterCriticalSection(&cs);
    DWORD now = GetTickCount();
    if (!ever || (now - cached_at) >= FL_VOL_TTL_MS) {
        cached = vol_query_raw();
        cached_at = now;
        ever = TRUE;
    }
    int out = cached;
    LeaveCriticalSection(&cs);
    { PROFILE_END_VAR(_pt, "fl_get_current_volume_platform"); return out; }
}
bool fl_set_volume_platform(int volume) {
        PROFILE_START(_pt);
HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    bool success = false;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) { PROFILE_END_VAR(_pt, "fl_set_volume_platform"); return false; }

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    float volume_float = (float)volume / 100.0f;

    hr = pVolume->lpVtbl->SetMasterVolumeLevelScalar(pVolume, volume_float, NULL);
    if (SUCCEEDED(hr)) {
        success = true;
    }

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();

    { PROFILE_END_VAR(_pt, "fl_set_volume_platform"); return success; }
}

bool fl_type_text_platform(const char *s){
  PROFILE_START(_pt);
if(!s) { PROFILE_END_VAR(_pt, "fl_type_text_platform"); return false; }
 Platform_EmitKey(VK_CONTROL,0); Platform_EmitKey(VK_LCONTROL,0); Platform_EmitKey(VK_RCONTROL,0);
 Platform_EmitKey(VK_SHIFT,0); Platform_EmitKey(VK_LSHIFT,0); Platform_EmitKey(VK_RSHIFT,0);
 Platform_EmitKey(VK_MENU,0); Platform_EmitKey(VK_LMENU,0); Platform_EmitKey(VK_RMENU,0);
 Platform_EmitKey(VK_LWIN,0); Platform_EmitKey(VK_RWIN,0);
 Sleep(20);
 for(;*s;s++){ SHORT sc=VkKeyScanA(*s); if(sc==-1){ if(*s=='\n'){ Platform_EmitKey(VK_RETURN,1); Platform_EmitKey(VK_RETURN,0); } else if(*s=='\t'){ Platform_EmitKey(VK_TAB,1); Platform_EmitKey(VK_TAB,0); } continue; } BYTE vk=LOBYTE(sc); BYTE sh=HIBYTE(sc); INPUT ins[4]={0}; int n=0; if(sh&1){ ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=VK_SHIFT; n++; } ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=vk; n++; ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=vk; ins[n].ki.dwFlags=KEYEVENTF_KEYUP; n++; if(sh&1){ ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=VK_SHIFT; ins[n].ki.dwFlags=KEYEVENTF_KEYUP; n++; } SendInput(n, ins, sizeof(INPUT)); Sleep(5); }
 { PROFILE_END_VAR(_pt, "fl_type_text_platform"); return true; }
}
static bool g_edit_placed_win = false;

void fl_editbox_reset_placed(void) {     PROFILE_START(_pt);
g_edit_placed_win = false; }

bool fl_editbox_was_placed(void) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_editbox_was_placed"); return g_edit_placed_win; } }

bool fl_place_editbox_platform(const FlEditSpec *spec) {
        PROFILE_START(_pt);
if (!spec) { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return false; }
    g_edit_placed_win = true;
    HDC hdc = NULL;
    Platform_GetDrawContextWin32(&hdc);
    if (!hdc) { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return false; }
    extern PromptState* Platform_GetWin32PromptState(void);
    PromptState *state = Platform_GetWin32PromptState();
    const char *buf = spec->buffer;
    if (!buf) {
        if (state) buf = state->filter;
        else buf = "";
    }
    int x = spec->x;
    int y = spec->y;
    int w = spec->w;
    int h = spec->h;
    if (w <= 0) w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
    if (w < 40) w = 40;
    if (h <= 0) h = PromptCustomEditHeight();
    HFONT hfont = spec->font.hfont ? spec->font.hfont : (g_win32_hMainFont ? g_win32_hMainFont : NULL);
    HFONT old = NULL;
    if (hfont) old = (HFONT)SelectObject(hdc, hfont);
    SetBkMode(hdc, TRANSPARENT);
    unsigned long text_hex = spec->color ? spec->color : COLOR_TEXT;
    SetTextColor(hdc, text_hex);
    int text_len = (int)strlen(buf);
    int cursor = state ? state->cursor : text_len;
    if (cursor < 0) cursor = 0;
    if (cursor > text_len) cursor = text_len;
    int visible_w = w - PROMPT_EDIT_SCROLL_PAD;
    int cursor_x = 0;
    int scroll = state ? state->scroll_offset : 0;
    if (hfont) {
        TEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        GetTextMetrics(hdc, &tm);
        int textH = tm.tmHeight > 0 ? tm.tmHeight : FontSize;
        int y_raw = (spec->y == 0) ? PROMPT_EDIT_INSET : spec->y;
        SIZE szCur; GetTextExtentPoint32A(hdc, buf, cursor, &szCur); cursor_x = szCur.cx;
        SIZE szText; GetTextExtentPoint32A(hdc, buf, text_len, &szText); int text_w = szText.cx;
        if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
        if (cursor_x - scroll < 0) scroll = cursor_x;
        if (scroll < 0) scroll = 0;
        if (text_w - scroll < visible_w && scroll > 0) { if (text_w < visible_w) scroll = 0; else scroll = text_w - visible_w; }
        if (state) state->scroll_offset = scroll;
        int cursor_draw_x = x + PROMPT_EDIT_INSET + cursor_x - scroll;
        if (cursor_draw_x < x + PROMPT_CARET_LO_CLAMP) cursor_draw_x = x + PROMPT_CARET_LO_CLAMP;
        if (cursor_draw_x > x + w - PROMPT_CARET_HI_CLAMP) cursor_draw_x = x + w - PROMPT_CARET_HI_CLAMP;
        int cursorH = FontSize;
        int cursor_y = y_raw + (textH - cursorH) / 2;
        if (cursor_y < y_raw) cursor_y = y_raw;
        RECT textRc = {x + PROMPT_EDIT_INSET - scroll, y_raw, x + w, y_raw + textH};
        if (!IsInsertMode()) {
            int tint_y = y_raw + (textH - cursorH) / 2;
            if (tint_y < y_raw) tint_y = y_raw;
            COLORREF tint = LerpColor(COLOR_BG, Modal_CursorColor(), 0.72f);
            int tint_x = x + PROMPT_EDIT_INSET + cursor_x - scroll;
            if (tint_x < x + PROMPT_CARET_LO_CLAMP) tint_x = x + PROMPT_CARET_LO_CLAMP;
            if (tint_x > x + w - PROMPT_CARET_HI_CLAMP) tint_x = x + w - PROMPT_CARET_HI_CLAMP;
            int sel_start = 0, sel_end = 0;
            bool has_sel = VimState_GetSelection(&sel_start, &sel_end) && text_len > 0;
            RECT tintRc;
            if (has_sel) {
                if (sel_start < 0) sel_start = 0;
                if (sel_end >= text_len) sel_end = text_len - 1;
                if (sel_start <= sel_end) {
                    int sx0 = x + PROMPT_EDIT_INSET + Platform_MeasureTextWidth(buf, sel_start, FontSize) - scroll;
                    int sx1 = x + PROMPT_EDIT_INSET + Platform_MeasureTextWidth(buf, sel_end + 1, FontSize) - scroll;
                    if (sx0 < x + PROMPT_CARET_LO_CLAMP) sx0 = x + PROMPT_CARET_LO_CLAMP;
                    if (sx1 > x + w - PROMPT_CARET_HI_CLAMP) sx1 = x + w - PROMPT_CARET_HI_CLAMP;
                    int cx = tint_x;
                    if (cx < sx0) cx = sx0;
                    if (cx > sx1) cx = sx1;
                    int sw = sx1 - sx0;
                    tintRc = (RECT){sx0, tint_y, sx0 + sw, tint_y + cursorH};
                }
            } else {
                tintRc = (RECT){tint_x, tint_y, tint_x + PromptNormalCaretWidth(), tint_y + cursorH};
            }
            HBRUSH tbr = CreateSolidBrush(tint);
            FillRect(hdc, &tintRc, tbr);
            DeleteObject(tbr);
        }
        DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_TOP | DT_LEFT);
        if (IsInsertMode()) {
            HBRUSH cbr = CreateSolidBrush(Modal_CursorColor());
            RECT cRc = {cursor_draw_x, cursor_y, cursor_draw_x + PROMPT_CARET_W_INSERT, cursor_y + cursorH};
            FillRect(hdc, &cRc, cbr);
            DeleteObject(cbr);
        }
    } else {
        RECT textRc = {x + PROMPT_EDIT_INSET - scroll, y, x + w, y + h};
        DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    if (old) SelectObject(hdc, old);
    { PROFILE_END_VAR(_pt, "fl_place_editbox_platform"); return true; }
}

typedef struct {
    void (*callback)(void);
} TimerPayload;

VOID CALLBACK TimerTrampoline(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
        PROFILE_START(_pt);
TimerPayload* payload = (TimerPayload*)lpParam;
    if (payload) {
        if (payload->callback) {
            payload->callback();
        }
        free(payload);
    }
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
        PROFILE_START(_pt);
if (!callback || seconds < 0) { PROFILE_END_VAR(_pt, "fl_run_after_time_platform"); return ; }

    TimerPayload* payload = (TimerPayload*)malloc(sizeof(TimerPayload));
    if (!payload) { PROFILE_END_VAR(_pt, "fl_run_after_time_platform"); return ; }
    payload->callback = callback;

    DWORD delay_ms = (DWORD)(seconds * 1000.0);
    HANDLE hTimer = NULL;
    if (!CreateTimerQueueTimer(
            &hTimer,
            NULL,
            TimerTrampoline,
            payload,
            delay_ms,
            0,  
            WT_EXECUTEONLYONCE
        )) {
        free(payload);
        return; 
    }
}

FlTimer fl_start_timer_platform(void) {
        PROFILE_START(_pt);
FlTimer t = {0};
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    t.ticks = (uint64_t)c.QuadPart;
    { PROFILE_END_VAR(_pt, "fl_start_timer_platform"); return t; }
}

double fl_end_timer_platform(FlTimer t) {
        PROFILE_START(_pt);
LARGE_INTEGER now;
    LARGE_INTEGER freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (freq.QuadPart == 0) { PROFILE_END_VAR(_pt, "fl_end_timer_platform"); return 0.0; }
    uint64_t start = t.ticks;
    uint64_t end = (uint64_t)now.QuadPart;
    if (end < start) { PROFILE_END_VAR(_pt, "fl_end_timer_platform"); return 0.0; }
    { PROFILE_END_VAR(_pt, "fl_end_timer_platform"); return (double)(end - start) * 1000.0 / (double)freq.QuadPart; }
}

void fl_sleep_platform(double seconds) {
        PROFILE_START(_pt);
if (seconds <= 0) { PROFILE_END_VAR(_pt, "fl_sleep_platform"); return ; }
    double ms = seconds * 1000.0;
    if (ms > (double)0xFFFFFFFF) ms = (double)0xFFFFFFFF;
    Sleep((DWORD)ms);
}

typedef struct {
    void* (*fn)(void*);
    void* arg;
} FlWinThreadStart;

static DWORD WINAPI fl_win_thread_entry(LPVOID p) {
        PROFILE_START(_pt);
FlWinThreadStart* s = (FlWinThreadStart*)p;
    void* (*fn)(void*) = s->fn;
    void* arg = s->arg;
    free(s);
    fn(arg);
    { PROFILE_END_VAR(_pt, "fl_win_thread_entry"); return 0; }
}

void* fl_thread_create_platform(void* (*fn)(void*), void* arg) {
        PROFILE_START(_pt);
if (!fn) { PROFILE_END_VAR(_pt, "fl_thread_create_platform"); return NULL; }
    FlWinThreadStart* s = malloc(sizeof(*s));
    if (!s) { PROFILE_END_VAR(_pt, "fl_thread_create_platform"); return NULL; }
    s->fn = fn;
    s->arg = arg;
    HANDLE h = CreateThread(NULL, 0, fl_win_thread_entry, s, 0, NULL);
    if (!h) { free(s); return NULL; }
    { PROFILE_END_VAR(_pt, "fl_thread_create_platform"); return (void*)h; }
}

void fl_thread_join_platform(void* thread) {
        PROFILE_START(_pt);
if (!thread) { PROFILE_END_VAR(_pt, "fl_thread_join_platform"); return ; }
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
}

void* fl_mutex_create_platform(void) {
        PROFILE_START(_pt);
CRITICAL_SECTION* cs = malloc(sizeof(*cs));
    if (!cs) { PROFILE_END_VAR(_pt, "fl_mutex_create_platform"); return NULL; }
    InitializeCriticalSection(cs);
    { PROFILE_END_VAR(_pt, "fl_mutex_create_platform"); return cs; }
}

void fl_mutex_destroy_platform(void* mutex) {
        PROFILE_START(_pt);
if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_destroy_platform"); return ; }
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
    DeleteCriticalSection(cs);
    free(cs);
}

void fl_mutex_lock_platform(void* mutex) {
        PROFILE_START(_pt);
if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_lock_platform"); return ; }
    EnterCriticalSection((CRITICAL_SECTION*)mutex);
}

void fl_mutex_unlock_platform(void* mutex) {
        PROFILE_START(_pt);
if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_unlock_platform"); return ; }
    LeaveCriticalSection((CRITICAL_SECTION*)mutex);
}

int fl_get_active_interface_platform(char out[], size_t max_len) {
        PROFILE_START(_pt);
ULONG size = 15 * 1024;
    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!addrs) { PROFILE_END_VAR(_pt, "fl_get_active_interface_platform"); return 0; }

    DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, addrs, &size);
    if (ret != NO_ERROR) {
        free(addrs);
        return 0;
    }

    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->FirstGatewayAddress == NULL) continue;

        const char* id = a->AdapterName;
        if (!id[0]) continue;
        size_t len = strlen(id);
        if (len >= max_len) len = max_len - 1;
        memcpy(out, id, len);
        out[len] = '\0';
        free(addrs);
        return 1;
    }

    free(addrs);
    out[0] = '\0';
    { PROFILE_END_VAR(_pt, "fl_get_active_interface_platform"); return 0; }
}

unsigned long long fl_net_rx_bytes_platform() {
        PROFILE_START(_pt);
char iface[64];
    if (!fl_get_active_interface_platform(iface, sizeof(iface)) || !iface[0]) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return 0; }
    MIB_IFTABLE* table = NULL;
    DWORD size = 0;
    
    if (GetIfTable(NULL, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return 0; }
    
    table = (MIB_IFTABLE*)malloc(size);
    if (!table) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return 0; }
    
    if (GetIfTable(table, &size, FALSE) != NO_ERROR) {
        free(table);
        return 0;
    }
    
    unsigned long long rx = 0;
    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        MIB_IFROW* row = &table->table[i];
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        
        if (iface && iface[0]) {
            char name[256];
            wcstombs(name, row->wszName, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            const char* guid = strrchr(name, '{');
            if (!guid || _stricmp(guid, iface) != 0) continue;
        }
        rx += row->dwInOctets;
    }
    
    free(table);
    { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_platform"); return rx; }
}

static CRITICAL_SECTION g_net_cs;
static BOOL g_net_cs_ready = FALSE;
static FlNetStatus g_net_cached = FL_NET_OFFLINE;
static DWORD g_net_cached_at = 0;
static DWORD g_net_ever = 0;

#define FL_NET_TTL_MS 5000

static void net_lock(void) {
        PROFILE_START(_pt);
if (!g_net_cs_ready) {
        InitializeCriticalSection(&g_net_cs);
        g_net_cs_ready = TRUE;
    }
    EnterCriticalSection(&g_net_cs);
}

static void net_unlock(void) {
        PROFILE_START(_pt);
if (g_net_cs_ready) LeaveCriticalSection(&g_net_cs);
}

static void net_refresh(void) {
        PROFILE_START(_pt);
g_net_cached = FL_NET_OFFLINE;

    ULONG size = 15 * 1024;
    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!addrs) { PROFILE_END_VAR(_pt, "net_refresh"); return ; }
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, addrs, &size) != NO_ERROR) {
        free(addrs);
        return;
    }

    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->FirstGatewayAddress == NULL) continue;

        if (a->IfType == IF_TYPE_IEEE80211) g_net_cached = FL_NET_WIFI;
        else if (a->IfType == IF_TYPE_ETHERNET_CSMACD) g_net_cached = FL_NET_ETHERNET;
        break;
    }

    free(addrs);
}

FlNetStatus fl_net_status_platform(void) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "fl_net_status_platform"); return st; }
}

bool fl_dismiss_message_platform() {
    if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
      DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
      FlintContext_Get()->window.hDisplayMessage = WH_NULL;
      return true;
    }
    return false;
}

/* ---- HTTP (WinHTTP) ---- */

#define FL_HTTP_CONNECT_TIMEOUT_S 10
#define FL_HTTP_TIMEOUT_S 30
#define FL_HTTP_MAX_REDIRECTS 10
#define FL_HTTP_DEFAULT_LIMIT ((size_t)64 * 1024 * 1024)

typedef struct { char *buf; size_t len; size_t cap; size_t limit; int overflow; } FlHttpBuf;

static int fl_http_buf_append(FlHttpBuf *b, const char *data, size_t n) {
    if (b->limit && b->len + n > b->limit) {
        size_t room = b->limit > b->len ? b->limit - b->len : 0;
        if (room > 0) { memcpy(b->buf + b->len, data, room); b->len += room; b->buf[b->len] = '\0'; }
        b->overflow = 1;
        return 0;
    }
    size_t need = b->len + n + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (cap < need) cap *= 2;
        char *grown = (char *)realloc(b->buf, cap);
        if (!grown) return 0;
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, data, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static wchar_t *fl_http_widen(const char *s) {
    if (!s || !s[0]) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static void fl_http_url_parts(const char *url, wchar_t **out_host, wchar_t **out_path, int *out_secure) {
    *out_host = NULL; *out_path = NULL; *out_secure = 0;
    const char *p = url;
    if (Platform_StringStartsWithCaseInsensitive(p, "https://")) { *out_secure = 1; p += 8; }
    else if (Platform_StringStartsWithCaseInsensitive(p, "http://")) { p += 7; }
    else return;
    const char *slash = strchr(p, '/');
    char hostbuf[256];
    if (slash) {
        size_t hl = (size_t)(slash - p);
        if (hl >= sizeof(hostbuf)) hl = sizeof(hostbuf) - 1;
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
    if (!cached) cached = WinHttpOpen(L"Flint/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    return cached;
}

static int fl_http_perform(const char *url, const char *method, const char *content_type,
                           const char *body, FlHttpBuf *buf, FILE *out_file, int redirects_left) {
    wchar_t *host = NULL, *path = NULL;
    int secure = 0;
    fl_http_url_parts(url, &host, &path, &secure);
    if (!host || !path) { free(host); free(path); return 0; }
    HINTERNET hs = fl_http_session();
    if (!hs) { free(host); free(path); return 0; }
    HINTERNET hc = WinHttpConnect(hs, host, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hc) { free(host); free(path); return 0; }
    wchar_t wmethod[16];
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wmethod, 16);
    HINTERNET hr = WinHttpOpenRequest(hc, wmethod, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    free(host);
    free(path);
    if (!hr) { WinHttpCloseHandle(hc); return 0; }
    DWORD t = FL_HTTP_TIMEOUT_S * 1000;
    WinHttpSetTimeouts(hr, (DWORD)(FL_HTTP_CONNECT_TIMEOUT_S * 1000), t, t, t);
    int ok = 0;
    if (body && body[0]) {
        size_t blen = strlen(body);
        wchar_t *wct = content_type && content_type[0] ? fl_http_widen(content_type) : NULL;
        ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body, (DWORD)blen, (DWORD)blen, 0) ? 1 : 0;
        if (ok && wct) {
            wchar_t hdr[256];
            _snwprintf(hdr, 256, L"Content-Type: %s", wct);
            hdr[255] = 0;
            ok = WinHttpAddRequestHeaders(hr, hdr, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD) ? 1 : 0;
        }
        free(wct);
        if (ok) ok = WinHttpReceiveResponse(hr, NULL) ? 1 : 0;
    } else {
        ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ? 1 : 0;
        if (ok) ok = WinHttpReceiveResponse(hr, NULL) ? 1 : 0;
    }
    if (ok) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if ((status >= 300 && status < 400) && redirects_left > 0) {
            wchar_t *loc = NULL; DWORD locsz = 0;
            if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &locsz, WINHTTP_NO_HEADER_INDEX) && locsz > 0) {
                loc = (wchar_t *)malloc(locsz);
                if (loc) {
                    WinHttpQueryHeaders(hr, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, loc, &locsz, WINHTTP_NO_HEADER_INDEX);
                    char narrow[2048];
                    WideCharToMultiByte(CP_UTF8, 0, loc, -1, narrow, sizeof(narrow), NULL, NULL);
                    WinHttpCloseHandle(hr);
                    WinHttpCloseHandle(hc);
                    free(loc);
                    int r = fl_http_perform(narrow, method, content_type, body, buf, out_file, redirects_left - 1);
                    return r;
                }
            }
            ok = 1;
        } else {
            DWORD avail = 0;
            while (ok && WinHttpQueryDataAvailable(hr, &avail) && avail > 0) {
                DWORD want = avail > 65536 ? 65536 : avail;
                char *chunk = (char *)malloc(want);
                if (!chunk) { ok = 0; break; }
                DWORD got = 0;
                if (!WinHttpReadData(hr, chunk, want, &got) || got == 0) { free(chunk); break; }
                if (out_file) { fwrite(chunk, 1, got, out_file); }
                else if (!fl_http_buf_append(buf, chunk, got)) { free(chunk); ok = 0; break; }
                free(chunk);
            }
        }
    }
    WinHttpCloseHandle(hr);
    WinHttpCloseHandle(hc);
    return ok;
}

