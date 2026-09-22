#include <initguid.h> 
#include "platform/win32/win32_state.h"
#include <windows.h>
#include <ctype.h>
#include "deps/log.h"
#include <shlwapi.h>
#include "core/flapi_platform.h"
#include "core/platform.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/modal.h"
#include "config.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {

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
                fl_get_all_files_from_dir_platform(full_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(find_data.cFileName, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        if (Platform_StringContainsCaseInsensitive(ext, list_of_allowed_extensions[i])) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }

            if (allowed) {
                if (list_of_files->count >= list_of_files->capacity) {
                    list_of_files->capacity = list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;
                    list_of_files->items = realloc(list_of_files->items, list_of_files->capacity * sizeof(FileItem));
                }

                FileItem* item = &list_of_files->items[list_of_files->count];
                strncpy(item->path, full_path, sizeof(item->path) - 1);
                item->path[sizeof(item->path) - 1] = '\0';

                char clean_name[256];
                strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                clean_name[sizeof(clean_name) - 1] = '\0';
                char* dot = strrchr(clean_name, '.');
                if (dot) *dot = '\0';
                strncpy(item->name, clean_name, sizeof(item->name) - 1);
                item->name[sizeof(item->name) - 1] = '\0';

                list_of_files->count++;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return true;
}

static const char* Win32_HostForSystemFile(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return NULL;
    if (_stricmp(dot, ".cpl") == 0) return "control.exe";
    if (_stricmp(dot, ".msc") == 0) return "mmc.exe";
    return NULL;
}

bool fl_launch_with_args_platform(const char* path, const char* args) {
    if (!path) return false;
    char expanded[1024];
    extern bool Platform_ExpandPath(const char* t, char* o, size_t s);
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    path = expanded;
    char cmd[2048];
    if (args && args[0] && strcmp(args, "0") != 0) {
        snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, args);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    }
    const char* host = Win32_HostForSystemFile(path);
    if (host) {
        char hostParams[2048];
        if (args && args[0] && strcmp(args, "0") != 0) snprintf(hostParams, sizeof(hostParams), "\"%s\" %s", path, args);
        else snprintf(hostParams, sizeof(hostParams), "\"%s\"", path);
        HINSTANCE hres = ShellExecuteA(NULL, "open", host, hostParams, NULL, SW_SHOWNORMAL);
        if ((intptr_t)hres > 32) return true;
        log_warn("fl_launch host failed host='%s' params='%s' res=%d", host, hostParams, (int)(intptr_t)hres);
    }
    HINSTANCE res = ShellExecuteA(NULL, "open", path, args && args[0] ? args : NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res > 32) return true;
    log_warn("fl_launch ShellExecuteA failed path='%s' args='%s' res=%d", path, args ? args : "", (int)(intptr_t)res);
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
    const char* host = Win32_HostForSystemFile(path);
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
    g_fl_list_itemH = spec->item_height ? spec->item_height : 32;
    g_fl_list_y0 = spec->y ? spec->y : 70;
    int bottomMargin = (WindowTitleText || VimModeText) ? 60 : 12;
    int list_h;
    if (spec->h) list_h = spec->h;
    else list_h = st ? (st->height - g_fl_list_y0 - bottomMargin) : 400;
    if (!spec->x && !spec->w) {
        g_fl_list_bx = 20;
        g_fl_list_bw = (st ? st->width - 40 : 650);
        g_fl_list_fw = st ? st->width - 40 : 900;
    } else {
        g_fl_list_bx = spec->x;
        g_fl_list_bw = spec->w ? spec->w : 650;
        g_fl_list_fw = spec->w ? spec->w : 900;
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
        int R=6; if (R > CornerRadius) R = CornerRadius;
        int t = 1;
        int bx = g_fl_list_bx;
        int bw = g_fl_list_bw;
        RECT inner = {bx+t, y+1+t, bx+bw-t, y+g_fl_list_itemH-1-t};
        int innerR = R-1; if (innerR < 0) innerR = 0;
        if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = (RECT){bx, y, bx+bw, y+g_fl_list_itemH}; innerR = R; }
        {
            RECT fullRc = {bx, y, bx+bw, y+g_fl_list_itemH};
            if (EnableShaders && ShaderPreset != 0) {
                HWND hWin2 = WindowFromDC(g_fl_list_hdc);
                RECT winRc2 = {0,0, bw, g_fl_list_visible * g_fl_list_itemH};
                int winW2 = bw, winH2 = g_fl_list_visible * g_fl_list_itemH;
                if (hWin2) { GetClientRect(hWin2, &winRc2); winW2 = winRc2.right - winRc2.left; winH2 = winRc2.bottom - winRc2.top; if(winW2<=0) winW2=bw; if(winH2<=0) winH2=g_fl_list_visible * g_fl_list_itemH; }
                Win32_DrawBackgroundSlice(g_fl_list_hdc, &fullRc, COLOR_BG, winW2, winH2, fullRc.left, fullRc.top);
            } else {
                HBRUSH br = CreateSolidBrush(COLOR_BG);
                FillRect(g_fl_list_hdc, &fullRc, br);
                DeleteObject(br);
            }
        }
        if (sel) {
            Win32_FillRoundedRect(g_fl_list_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
        } else {
            if (EnableShaders && ShaderPreset != 0) {
                HWND hWin = WindowFromDC(g_fl_list_hdc);
                RECT winRc = {0,0, bw, g_fl_list_visible * g_fl_list_itemH};
                int winW = bw, winH = g_fl_list_visible * g_fl_list_itemH;
                if (hWin) { GetClientRect(hWin, &winRc); winW = winRc.right - winRc.left; winH = winRc.bottom - winRc.top; if (winW<=0) winW=bw; if(winH<=0) winH=g_fl_list_visible * g_fl_list_itemH; }
                int offsetX = inner.left;
                int offsetY = inner.top;
                Win32_FillRoundedRectSlice(g_fl_list_hdc, &inner, COLOR_BG, winW, winH, offsetX, offsetY, innerR);
            } else {
                Win32_FillRoundedRect(g_fl_list_hdc, &inner, COLOR_BG, innerR);
            }
        }
    } else {
        RECT rc = { g_fl_list_bx, y, g_fl_list_bx + g_fl_list_bw, y + g_fl_list_itemH };
        if (sel) {
            HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
            FillRect(g_fl_list_hdc, &rc, br);
            DeleteObject(br);
        } else {
            if (EnableShaders && ShaderPreset != 0) {
                HWND hWin = WindowFromDC(g_fl_list_hdc);
                RECT winRc = {0,0,900,400};
                int winW = 900, winH = 400;
                if (hWin) { GetClientRect(hWin, &winRc); winW = winRc.right - winRc.left; winH = winRc.bottom - winRc.top; if(winW<=0) winW=900; if(winH<=0) winH=400; }
                int offsetX = rc.left;
                int offsetY = rc.top;
                Win32_DrawBackgroundSlice(g_fl_list_hdc, &rc, COLOR_BG, winW, winH, offsetX, offsetY);
            } else {
                HBRUSH br = CreateSolidBrush(COLOR_BG);
                FillRect(g_fl_list_hdc, &rc, br);
                DeleteObject(br);
            }
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

int fl_get_current_volume_platform(void) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    float currentVolume = 0.0f;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) return 0;

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

bool fl_decrement_volume_by_platform(int decrement) {
    int current_volume = fl_get_current_volume_platform();
    
    int target_volume = current_volume - decrement;
    
    return fl_set_volume_platform(target_volume);
}

bool fl_increment_volume_by_platform(int increment) {
    int current_volume = fl_get_current_volume_platform();
    
    int target_volume = current_volume + increment;
    
    return fl_set_volume_platform(target_volume);
}

bool fl_decrement_volume_by_percent_platform(int decrement_percent) {
    int current_volume = fl_get_current_volume_platform();
    int target_volume = current_volume - (current_volume * decrement_percent / 100);
    return fl_set_volume_platform(target_volume);
}

bool fl_increment_volume_by_percent_platform(int increment_percent) {
    int current_volume = fl_get_current_volume_platform();
    int target_volume = current_volume + (current_volume * increment_percent / 100);
    return fl_set_volume_platform(target_volume);
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
    int x = spec->x ? spec->x : 20;
    int y = spec->y ? spec->y : 20;
    int w = spec->w;
    int h = spec->h;
    if (w <= 0) w = state ? state->width - 40 : 880;
    if (w < 40) w = 40;
    if (h <= 0) h = 32;
    RECT rc = {x, y, x + w, y + h};
    if (RoundedCorners && CornerRadius > 0) {
        Win32_FillRoundedRect(hdc, &rc, COLOR_BG, 6);
    } else {
        HBRUSH br = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
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
    int visible_w = w - 28;
    int cursor_x = 0;
    int scroll = state ? state->scroll_offset : 0;
    if (hfont) {
        TEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        GetTextMetrics(hdc, &tm);
        int textH = tm.tmHeight > 0 ? tm.tmHeight : FontSize;
        if (textH > h - 4) textH = h - 4;
        if (textH <= 0) textH = FontSize;
        SIZE szCur; GetTextExtentPoint32A(hdc, buf, cursor, &szCur); cursor_x = szCur.cx;
        SIZE szText; GetTextExtentPoint32A(hdc, buf, text_len, &szText); int text_w = szText.cx;
        if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
        if (cursor_x - scroll < 0) scroll = cursor_x;
        if (scroll < 0) scroll = 0;
        if (text_w - scroll < visible_w && scroll > 0) { if (text_w < visible_w) scroll = 0; else scroll = text_w - visible_w; }
        if (state) state->scroll_offset = scroll;
        RECT textRc = {x + 14 - scroll, y, x + w, y + h};
        DrawTextA(hdc, buf, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        int cursor_draw_x = x + 14 + cursor_x - scroll;
        if (cursor_draw_x < x + 2) cursor_draw_x = x + 2;
        if (cursor_draw_x > x + w - 4) cursor_draw_x = x + w - 4;
        int cursor_y = y + (h - textH) / 2;
        if (cursor_y < y + 2) cursor_y = y + 2;
        HBRUSH cbr = CreateSolidBrush(text_hex);
        RECT cRc;
        if (IsInsertMode()) cRc = (RECT){cursor_draw_x, cursor_y, cursor_draw_x+2, cursor_y+textH};
        else cRc = (RECT){cursor_draw_x, cursor_y, cursor_draw_x+12, cursor_y+textH};
        FillRect(hdc, &cRc, cbr);
        DeleteObject(cbr);
    } else {
        RECT textRc = {x + 14 - scroll, y, x + w, y + h};
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

#define FL_CLIPBOARD_HISTORY_MAX 64

static const unsigned char FL_PNG_MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

static FlClipboardHistory g_fl_clipboard_history = {0};

static void fl_clipboard_entry_free(FlClipboardEntry* e) {
    if (!e) return;
    free(e->text);
    e->text = NULL;
    free(e->image_path);
    e->image_path = NULL;
}

static bool fl_clipboard_history_push(FlClipboardEntry e) {
    FlClipboardHistory* h = &g_fl_clipboard_history;
    if (h->count >= FL_CLIPBOARD_HISTORY_MAX) {
        fl_clipboard_entry_free(&h->entries[0]);
        memmove(h->entries, h->entries + 1, (h->count - 1) * sizeof(h->entries[0]));
        h->count--;
    }
    if (h->count >= h->capacity) {
        size_t ncap = h->capacity ? h->capacity * 2 : 16;
        if (ncap > FL_CLIPBOARD_HISTORY_MAX) ncap = FL_CLIPBOARD_HISTORY_MAX;
        FlClipboardEntry* n = realloc(h->entries, ncap * sizeof(*n));
        if (!n) {
            fl_clipboard_entry_free(&e);
            return false;
        }
        h->entries = n;
        h->capacity = ncap;
    }
    h->entries[h->count++] = e;
    return true;
}

static void fl_clipboard_png_dims(const unsigned char* png, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!png || len < 24) return;
    if (memcmp(png, FL_PNG_MAGIC, 8) != 0) return;
    if (memcmp(png + 12, "IHDR", 4) != 0) return;
    uint32_t w = ((uint32_t)png[16] << 24) | ((uint32_t)png[17] << 16) | ((uint32_t)png[18] << 8) | png[19];
    uint32_t h = ((uint32_t)png[20] << 24) | ((uint32_t)png[21] << 16) | ((uint32_t)png[22] << 8) | png[23];
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

bool fl_clipboard_history_push_text_platform(const char* text) {
    FlClipboardEntry e = {0};
    e.type = FL_CLIPBOARD_TEXT;
    e.timestamp = (uint64_t)time(NULL);
    e.text = text ? strdup(text) : NULL;
    if (text && !e.text) return false;
    if (!fl_clipboard_history_push(e)) return false;
    return true;
}

bool fl_clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h) {
    FlClipboardEntry e = {0};
    e.type = FL_CLIPBOARD_IMAGE;
    e.timestamp = (uint64_t)time(NULL);
    e.image_path = path ? strdup(path) : NULL;
    if (path && !e.image_path) return false;
    e.image_width = w;
    e.image_height = h;
    if (!fl_clipboard_history_push(e)) return false;
    return true;
}

size_t fl_clipboard_history_count_platform(void) {
    return g_fl_clipboard_history.count;
}

const FlClipboardEntry* fl_clipboard_history_get_platform(size_t index) {
    if (index >= g_fl_clipboard_history.count) return NULL;
    return &g_fl_clipboard_history.entries[index];
}

void fl_clipboard_clear_history_platform(void) {
    for (size_t i = 0; i < g_fl_clipboard_history.count; i++)
        fl_clipboard_entry_free(&g_fl_clipboard_history.entries[i]);
    free(g_fl_clipboard_history.entries);
    g_fl_clipboard_history.entries = NULL;
    g_fl_clipboard_history.count = 0;
    g_fl_clipboard_history.capacity = 0;
}

bool fl_clipboard_set_png_path_platform(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (32 * 1024 * 1024)) {
        fclose(f);
        return 0;
    }
    unsigned char* buf = malloc((size_t)n);
    if (!buf) {
        fclose(f);
        return 0;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    bool ok = 0;
    if (got == (size_t)n && memcmp(buf, FL_PNG_MAGIC, 8) == 0) {
        uint32_t w = 0, h = 0;
        fl_clipboard_png_dims(buf, (size_t)n, &w, &h);
        ok = Platform_CopyImageToClipboard(buf, (size_t)n);
        if (ok) fl_clipboard_history_push_image_platform(path, w, h);
    }
    free(buf);
    remove(path);
    return ok;
}

static void fl_win32_capture_primary(const char* fullPath) {
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    if (cx <= 0 || cy <= 0) return;
    Platform_SaveRegionToPNG(0, 0, cx, cy, fullPath);
}

void fl_screenshot_copy_area_platform(void) {
    char* wd = Platform_GetWorkingDir();
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");
    log_info("area select not implemented, capturing primary monitor");
    fl_win32_capture_primary(path);
    fl_clipboard_set_png_path_platform(path);
}

void fl_screenshot_copy_platform(void) {
    char* wd = Platform_GetWorkingDir();
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");
    fl_win32_capture_primary(path);
    fl_clipboard_set_png_path_platform(path);
}

void fl_screenshot_save_area_platform(char* save_path) {
    if (!save_path) return;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s.png", save_path);
    log_info("area select not implemented, capturing primary monitor");
    fl_win32_capture_primary(path);
}

void fl_screenshot_save_platform(char* save_path) {
    if (!save_path) return;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s.png", save_path);
    fl_win32_capture_primary(path);
}

void fl_ocr_area_to_clipboard_platform(void) {
    char* wd = Platform_GetWorkingDir();
    char png[MAX_PATH];
    snprintf(png, sizeof(png), "%s/flint_ocr.png", wd ? wd : ".");
    fl_win32_capture_primary(png);
    char cmd[2 * MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "tesseract \"%s\" stdout -l eng", png);
    char* out = Platform_RunCommandAndGetOutput(cmd);
    remove(png);
    if (!out || !out[0]) { free(out); return; }
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    if (out[0]) { if (Platform_CopyToClipboard(out)) fl_clipboard_history_push_text_platform(out); }
    free(out);
}
