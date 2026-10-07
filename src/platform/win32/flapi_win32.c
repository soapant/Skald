#include <winsock2.h>
#include <windows.h>
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
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool Platform_GetAllFilesFromDir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore, bool strip_extension) {

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return false;

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
    return true;
}

static const char* win32_host_for_system_file(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return NULL;
    if (_stricmp(dot, ".cpl") == 0) return "control.exe";
    if (_stricmp(dot, ".msc") == 0) return "mmc.exe";
    return NULL;
}

static bool win32_exec_shell(const char* file, const char* params, const char* working_dir, const char* verb) {
    SHELLEXECUTEINFOA sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = verb;
    sei.lpFile = file;
    sei.lpParameters = (params && params[0]) ? params : NULL;
    sei.lpDirectory = working_dir;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&sei)) return false;
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return true;
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
    if (!path) return false;
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
    if (win32_exec_shell(path, params[0] ? params : NULL, NULL, "open")) return true;
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
    return false;
}

bool fl_launch_with_args_as_admin_platform(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    extern bool Platform_ExpandPath(const char* t, char* o, size_t s);
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
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
    return ok;
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
    if (!spec) return false;
    ListBoxInfo *lb = spec->lb;
    if (!lb) {
        if (!spec->items || spec->count <= 0) return false;
        return false;
    }
    if (!lb->populated_count) return false;
    Platform_GetDrawContextWin32(&g_fl_list_hdc);
    if (!g_fl_list_hdc) return false;
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
    return true;
}

bool fl_listbox_next_platform(FlListItemCtx *out) {
    if (!out || !g_fl_list_lb) return false;
    if (g_fl_list_cur >= g_fl_list_visible) return false;
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) return false;
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
    return true;
}

void fl_listbox_end_platform(void) {
    g_fl_list_lb = NULL;
    g_fl_list_visible = 0;
    g_fl_list_cur = 0;
}

int fl_listbox_get_count(void) {
    return g_fl_list_lb ? g_fl_list_lb->populated_count : 0;
}

unsigned long fl_listbox_get_current_standard_color(void) {
    if (!g_fl_list_lb || g_fl_list_cur == 0) return 0;
    int idx = g_fl_list_lb->view_offset + g_fl_list_cur - 1;
    if (idx < 0 || idx >= g_fl_list_lb->populated_count) return 0;
    bool sel = (idx == g_fl_list_lb->selected_index);
    if (sel) return rgb_to_hex(COLOR_TEXT);
    int orig_idx = idx;
    int orig_cnt = g_fl_list_lb->populated_count;
    if (idx >= 0 && idx < g_fl_list_lb->populated_count) {
        orig_idx = g_fl_list_lb->matching_indices[idx];
        orig_cnt = g_fl_list_lb->last_item_count > 0 ? g_fl_list_lb->last_item_count : g_fl_list_lb->populated_count;
    }
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(orig_idx, orig_cnt, &ncol, &bcol, &pcol);
    return rgb_to_hex(pcol);
}

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
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
    return NULL;
}
#define VK_MEDIA_PLAY_PAUSE 0xB3

void fl_toggle_play_pause_platform(void) {
    INPUT input[2] = {0};

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, input, sizeof(INPUT));
}

static int vol_query_raw(void) {
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

    return (int)(currentVolume * 100.0f + 0.5f);
}

#define FL_VOL_TTL_MS 500

int fl_get_current_volume_platform(void) {
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
    return out;
}
bool fl_set_volume_platform(int volume) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    bool success = false;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) return false;

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

    return success;
}

bool fl_type_text_platform(const char *s){
 if(!s) return false;
 Platform_EmitKey(VK_CONTROL,0); Platform_EmitKey(VK_LCONTROL,0); Platform_EmitKey(VK_RCONTROL,0);
 Platform_EmitKey(VK_SHIFT,0); Platform_EmitKey(VK_LSHIFT,0); Platform_EmitKey(VK_RSHIFT,0);
 Platform_EmitKey(VK_MENU,0); Platform_EmitKey(VK_LMENU,0); Platform_EmitKey(VK_RMENU,0);
 Platform_EmitKey(VK_LWIN,0); Platform_EmitKey(VK_RWIN,0);
 Sleep(20);
 for(;*s;s++){ SHORT sc=VkKeyScanA(*s); if(sc==-1){ if(*s=='\n'){ Platform_EmitKey(VK_RETURN,1); Platform_EmitKey(VK_RETURN,0); } else if(*s=='\t'){ Platform_EmitKey(VK_TAB,1); Platform_EmitKey(VK_TAB,0); } continue; } BYTE vk=LOBYTE(sc); BYTE sh=HIBYTE(sc); INPUT ins[4]={0}; int n=0; if(sh&1){ ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=VK_SHIFT; n++; } ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=vk; n++; ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=vk; ins[n].ki.dwFlags=KEYEVENTF_KEYUP; n++; if(sh&1){ ins[n].type=INPUT_KEYBOARD; ins[n].ki.wVk=VK_SHIFT; ins[n].ki.dwFlags=KEYEVENTF_KEYUP; n++; } SendInput(n, ins, sizeof(INPUT)); Sleep(5); }
 return true;
}
static bool g_edit_placed_win = false;

void fl_editbox_reset_placed(void) { g_edit_placed_win = false; }

bool fl_editbox_was_placed(void) { return g_edit_placed_win; }

bool fl_place_editbox_platform(const FlEditSpec *spec) {
    if (!spec) return false;
    g_edit_placed_win = true;
    HDC hdc = NULL;
    Platform_GetDrawContextWin32(&hdc);
    if (!hdc) return false;
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
        DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_TOP | DT_LEFT);
        HBRUSH cbr = CreateSolidBrush(COLOR_CURSOR);
        RECT cRc;
        if (IsInsertMode()) cRc = (RECT){cursor_draw_x, cursor_y, cursor_draw_x+PROMPT_CARET_W_INSERT, cursor_y+cursorH};
        else cRc = (RECT){cursor_draw_x, cursor_y, cursor_draw_x+PromptNormalCaretWidth(), cursor_y+cursorH};
        FillRect(hdc, &cRc, cbr);
        DeleteObject(cbr);
    } else {
        RECT textRc = {x + PROMPT_EDIT_INSET - scroll, y, x + w, y + h};
        DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    if (old) SelectObject(hdc, old);
    return true;
}

typedef struct {
    void (*callback)(void);
} TimerPayload;

VOID CALLBACK TimerTrampoline(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
    TimerPayload* payload = (TimerPayload*)lpParam;
    if (payload) {
        if (payload->callback) {
            payload->callback();
        }
        free(payload);
    }
}

void fl_run_after_time_platform(double seconds, void (*callback)(void)) {
    if (!callback || seconds < 0) return;

    TimerPayload* payload = (TimerPayload*)malloc(sizeof(TimerPayload));
    if (!payload) return;
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
    FlTimer t = {0};
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    t.ticks = (uint64_t)c.QuadPart;
    return t;
}

double fl_end_timer_platform(FlTimer t) {
    LARGE_INTEGER now;
    LARGE_INTEGER freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (freq.QuadPart == 0) return 0.0;
    uint64_t start = t.ticks;
    uint64_t end = (uint64_t)now.QuadPart;
    if (end < start) return 0.0;
    return (double)(end - start) * 1000.0 / (double)freq.QuadPart;
}

void fl_sleep_platform(double seconds) {
    if (seconds <= 0) return;
    double ms = seconds * 1000.0;
    if (ms > (double)0xFFFFFFFF) ms = (double)0xFFFFFFFF;
    Sleep((DWORD)ms);
}

typedef struct {
    void* (*fn)(void*);
    void* arg;
} FlWinThreadStart;

static DWORD WINAPI fl_win_thread_entry(LPVOID p) {
    FlWinThreadStart* s = (FlWinThreadStart*)p;
    void* (*fn)(void*) = s->fn;
    void* arg = s->arg;
    free(s);
    fn(arg);
    return 0;
}

void* fl_thread_create_platform(void* (*fn)(void*), void* arg) {
    if (!fn) return NULL;
    FlWinThreadStart* s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->fn = fn;
    s->arg = arg;
    HANDLE h = CreateThread(NULL, 0, fl_win_thread_entry, s, 0, NULL);
    if (!h) { free(s); return NULL; }
    return (void*)h;
}

void fl_thread_join_platform(void* thread) {
    if (!thread) return;
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
}

void* fl_mutex_create_platform(void) {
    CRITICAL_SECTION* cs = malloc(sizeof(*cs));
    if (!cs) return NULL;
    InitializeCriticalSection(cs);
    return cs;
}

void fl_mutex_destroy_platform(void* mutex) {
    if (!mutex) return;
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
    DeleteCriticalSection(cs);
    free(cs);
}

void fl_mutex_lock_platform(void* mutex) {
    if (!mutex) return;
    EnterCriticalSection((CRITICAL_SECTION*)mutex);
}

void fl_mutex_unlock_platform(void* mutex) {
    if (!mutex) return;
    LeaveCriticalSection((CRITICAL_SECTION*)mutex);
}

int fl_get_active_interface_platform(char out[], size_t max_len) {
    ULONG size = 15 * 1024;
    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!addrs) return 0;

    DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, addrs, &size);
    if (ret != NO_ERROR) {
        free(addrs);
        return 0;
    }

    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->FirstGatewayAddress == NULL) continue;

        size_t len = wcslen(a->FriendlyName);
        if (len >= max_len) len = max_len - 1;
        wcstombs(out, a->FriendlyName, len);
        out[len] = '\0';
        free(addrs);
        return 1;
    }

    free(addrs);
    out[0] = '\0';
    return 0;
}

unsigned long long fl_net_rx_bytes_platform() {
    char iface[64];
    if (!fl_get_active_interface_platform(iface, sizeof(iface)) || !iface[0]) return 0;
    MIB_IFTABLE* table = NULL;
    DWORD size = 0;
    
    if (GetIfTable(NULL, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) return 0;
    
    table = (MIB_IFTABLE*)malloc(size);
    if (!table) return 0;
    
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
            if (strcmp(name, iface) != 0) continue;
        }
        rx += row->dwInOctets;
    }
    
    free(table);
    return rx;
}

static CRITICAL_SECTION g_net_cs;
static BOOL g_net_cs_ready = FALSE;
static INetworkListManager* g_net_nlm = NULL;
static FlNetStatus g_net_cached = FL_NET_OFFLINE;
static DWORD g_net_cached_at = 0;
static DWORD g_net_ever = 0;

#define FL_NET_TTL_MS 5000

static void net_lock(void) {
    if (!g_net_cs_ready) {
        InitializeCriticalSection(&g_net_cs);
        g_net_cs_ready = TRUE;
    }
    EnterCriticalSection(&g_net_cs);
}

static void net_unlock(void) {
    if (g_net_cs_ready) LeaveCriticalSection(&g_net_cs);
}

#if defined(__GNUC__)
#define FL_NET_TLS __thread
#else
#define FL_NET_TLS __declspec(thread)
#endif

static bool net_com_ready(void) {
    static FL_NET_TLS bool ready = FALSE;
    if (ready) return true;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) { ready = TRUE; return true; }
    if (FAILED(hr)) return false;
    ready = TRUE;
    return true;
}

static INetworkListManager* net_nlm(void) {
    net_lock();
    if (!g_net_nlm && net_com_ready()) {
        INetworkListManager* nlm = NULL;
        HRESULT hr = CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_ALL,
                                      &IID_INetworkListManager, (void**)&nlm);
        if (SUCCEEDED(hr)) g_net_nlm = nlm;
    }
    INetworkListManager* nlm = g_net_nlm;
    net_unlock();
    return nlm;
}

static void net_guid_to_name(const GUID* g, char* out, size_t out_len) {
    snprintf(out, out_len,
             "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             (unsigned long)g->Data1, (unsigned)g->Data2, (unsigned)g->Data3,
             g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
             g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}

static bool net_adapter_type(const GUID* id, ULONG* out_type) {
    char want[64];
    net_guid_to_name(id, want, sizeof(want));

    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &size);
    if (size == 0) return false;
    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!addrs) return false;
    bool found = false;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addrs, &size) == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
            if (_stricmp(a->AdapterName, want) != 0) continue;
            *out_type = a->IfType;
            found = true;
            break;
        }
    }
    free(addrs);
    return found;
}

static void net_refresh(void) {
    g_net_cached = FL_NET_OFFLINE;

    INetworkListManager* nlm = net_nlm();
    if (!nlm) return;

    NLM_CONNECTIVITY conn = (NLM_CONNECTIVITY)0;
    if (FAILED(nlm->lpVtbl->GetConnectivity(nlm, &conn))) return;
    if (!(conn & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET))) return;

    IEnumNetworkConnections* conns = NULL;
    if (FAILED(nlm->lpVtbl->GetNetworkConnections(nlm, &conns))) return;

    INetworkConnection* c = NULL;
    ULONG fetched = 0;
    bool done = false;
    while (conns->lpVtbl->Next(conns, 1, &c, &fetched) == S_OK) {
        VARIANT_BOOL is_conn = VARIANT_FALSE;
        NLM_CONNECTIVITY ct = (NLM_CONNECTIVITY)0;
        GUID id;
        if (SUCCEEDED(c->lpVtbl->get_IsConnected(c, &is_conn)) && is_conn == VARIANT_TRUE &&
            SUCCEEDED(c->lpVtbl->GetConnectivity(c, &ct)) &&
            (ct & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET)) &&
            SUCCEEDED(c->lpVtbl->GetAdapterId(c, &id))) {
            ULONG t = 0;
            if (net_adapter_type(&id, &t)) {
                if (t == IF_TYPE_IEEE80211) g_net_cached = FL_NET_WIFI;
                else if (t == IF_TYPE_ETHERNET_CSMACD) g_net_cached = FL_NET_ETHERNET;
                done = true;
            }
        }
        c->lpVtbl->Release(c);
        c = NULL;
        if (done) break;
    }
    conns->lpVtbl->Release(conns);
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
