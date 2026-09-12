#include <windows.h>
#include <shlwapi.h>
#include "core/flapi.h"
#include "core/platform.h"


void fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

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
    HINSTANCE res = ShellExecuteA(NULL, "open", path, args && args[0] ? args : NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res > 32) return true;
    STARTUPINFOA si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return true;
    }
    return false;
}

bool fl_launch_with_args_as_admin_platform(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    extern bool Platform_ExpandPath(const char* t, char* o, size_t s);
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    path = expanded;
    char params[1024] = {0};
    if (args && args[0]) snprintf(params, sizeof(params), "%s", args);
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.lpParameters = params[0] ? params : NULL;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExA(&sei);
}

#include "platform/draw.h"
#include "core/common_ui.h"
#include "config.h"

static FlListSpec g_fl_list_spec;
static ListBoxInfo *g_fl_list_lb;
static int g_fl_list_y0, g_fl_list_itemH, g_fl_list_visible, g_fl_list_cur;
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
    g_fl_list_itemH = spec->item_height ? spec->item_height : FontSize + 14;
    g_fl_list_y0 = spec->y ? spec->y : 70;
    int list_h = spec->h ? spec->h : 400;
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
        int t = ListboxBorderThickness; if (t < 1) t = 1; if (t > 4) t = 4;
        int bx = g_fl_list_spec.x;
        int bw = (g_fl_list_spec.w ? g_fl_list_spec.w : 650);
        RECT inner = {bx+t, y+1+t, bx+bw-t, y+g_fl_list_itemH-1-t};
        int innerR = R-1; if (innerR < 0) innerR = 0;
        if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = (RECT){bx, y, bx+bw, y+g_fl_list_itemH}; innerR = R; }
        // Pre-fill full item rect with window background to avoid black gaps (continuous background)
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
            if (EnableSelectionShaders && SelectionShaderPreset != 0) {
                Win32_DrawRoundedSelection(g_fl_list_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
            } else {
                Win32_FillRoundedRect(g_fl_list_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
            }
        } else {
            if (EnableShaders && ShaderPreset != 0) {
                HWND hWin = WindowFromDC(g_fl_list_hdc);
                RECT winRc = {0,0, bw, g_fl_list_visible * g_fl_list_itemH};
                int winW = bw, winH = g_fl_list_visible * g_fl_list_itemH;
                if (hWin) { GetClientRect(hWin, &winRc); winW = winRc.right - winRc.left; winH = winRc.bottom - winRc.top; if (winW<=0) winW=bw; if(winH<=0) winH=g_fl_list_visible * g_fl_list_itemH; }
                int offsetX = inner.left;
                int offsetY = inner.top;
                // inner is already in window coords, use window container for continuous gradient
                Win32_FillRoundedRectSlice(g_fl_list_hdc, &inner, COLOR_BG, winW, winH, offsetX, offsetY, innerR);
            } else {
                Win32_FillRoundedRect(g_fl_list_hdc, &inner, COLOR_BG, innerR);
            }
        }
    } else {
        RECT rc = { g_fl_list_spec.x, y, g_fl_list_spec.x + (g_fl_list_spec.w ? g_fl_list_spec.w : 900), y + g_fl_list_itemH };
        if (sel) {
            if (EnableSelectionShaders && SelectionShaderPreset != 0) {
                Win32_DrawSelectionBackground(g_fl_list_hdc, &rc, COLOR_SELECTED_TEXT);
            } else {
                HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
                FillRect(g_fl_list_hdc, &rc, br);
                DeleteObject(br);
            }
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
    if (sel) return COLOR_TEXT;
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(idx, g_fl_list_lb->populated_count, &ncol, &bcol, &pcol);
    return pcol;
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
        if (Platform_StringContainsCaseInsensitive(find_data.cFileName, search_string) != NULL) {
            snprintf(found_path, sizeof(found_path), "%s\\%s", directory, find_data.cFileName);
            FindClose(hFind);
            return found_path;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return NULL;
}