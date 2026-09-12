#include "core/flapi.h"
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* app_name;  
} AppHotkey;

const char* ListOfShitToIgnore[] = {
  "node_modules",
  ".git",
  "bin",
  "dll",
  "gtnh 2.8",
  ".h",
  ".c",
  "yazi",
  "obj",
  "build",
};

const char* ListOfProgramPaths[] = {
#if defined(_WIN32)
    "{APPDATA}/Microsoft/Windows/Start Menu",
    "{PROGRAMDATA}/Microsoft/Windows/Start Menu/Programs",
    "{MISC}",
    "{SystemRoot}/System32/",             
    "shell:AppsFolder",
#else
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/home/diggy/.local/share/applications",
    "/usr/bin",
    "/home/diggy/.local/bin",
    "/home/diggy/miscellaneous",
#endif
};

const PathEntry SystemShortcuts[] = {
    {"control.exe", "Control Panel"},
    {"devmgmt.msc", "Device Manager"},
    {"compmgmt.msc", "Computer Management"},
    {"diskmgmt.msc", "Disk Management"},
    {"services.msc", "Services"},
    {"taskmgr.exe", "Task Manager"},
    {"notepad.exe", "Notepad"},
    {"calc.exe", "Calculator"},
    {"mspaint.exe", "Paint"},
    {"cmd.exe", "Command Prompt"},
    {"powershell.exe", "PowerShell"},
    {"msconfig.exe", "System Configuration"},
    {"dxdiag.exe", "DirectX Diagnostic Tool"},
    {"sysdm.cpl", "System Properties"},
    {"inetcpl.cpl", "Internet Options"},
    {"appwiz.cpl", "Programs and Features"},
    {"timedate.cpl", "Date and Time"},
    {"desk.cpl", "Display Settings"},
    {"main.cpl", "Mouse Properties"}
};

#define SYSTEM_SHORTCUT_COUNT ARRAY_LEN(SystemShortcuts)
#define MAX_APPS 2048

ListOfFiles s_apps = {0};
#if defined(_WIN32)
static CRITICAL_SECTION g_apps_cs;
static bool g_apps_cs_inited = false;
static void apps_lock_init(void) { if (!g_apps_cs_inited) { InitializeCriticalSection(&g_apps_cs); g_apps_cs_inited = true; } }
#define APPS_LOCK() do { apps_lock_init(); EnterCriticalSection(&g_apps_cs); } while(0)
#define APPS_UNLOCK() LeaveCriticalSection(&g_apps_cs)
#else
#define APPS_LOCK() do {} while(0)
#define APPS_UNLOCK() do {} while(0)
#endif
static char* s_app_icons[MAX_APPS] = {0};
static size_t s_app_icon_count = 0;
#if defined(_WIN32)
static HICON s_hicons[MAX_APPS] = {0};
static size_t s_hicon_count = 0;
static HICON win_load_icon_for_path(const char* path, int size) {
    if (!path || !path[0]) return NULL;
    SHFILEINFOA shfi = {0};
    DWORD flags = SHGFI_ICON | (size <= 16 ? SHGFI_SMALLICON : SHGFI_LARGEICON);
    if (SHGetFileInfoA(path, 0, &shfi, sizeof(shfi), flags)) return shfi.hIcon;
    if (SHGetFileInfoA(path, FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), flags | SHGFI_USEFILEATTRIBUTES)) return shfi.hIcon;
    // fallback: try large if small failed and vice versa
    DWORD alt = SHGFI_ICON | (size <= 16 ? SHGFI_LARGEICON : SHGFI_SMALLICON);
    if (SHGetFileInfoA(path, 0, &shfi, sizeof(shfi), alt)) return shfi.hIcon;
    if (SHGetFileInfoA(path, FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), alt | SHGFI_USEFILEATTRIBUTES)) return shfi.hIcon;
    return NULL;
}
#endif

static void free_icon_cache(void) {
    APPS_LOCK();
    for (size_t i = 0; i < s_app_icon_count; i++) {
        free(s_app_icons[i]);
        s_app_icons[i] = NULL;
    }
    s_app_icon_count = 0;
#if defined(_WIN32)
    for (size_t i = 0; i < s_hicon_count; i++) {
        if (s_hicons[i]) DestroyIcon(s_hicons[i]);
        s_hicons[i] = NULL;
    }
    s_hicon_count = 0;
#endif
    APPS_UNLOCK();
}

static void rebuild_icon_cache(void) {
    size_t n; APPS_LOCK(); n = s_apps.count < MAX_APPS ? s_apps.count : MAX_APPS; APPS_UNLOCK();
    char* new_icons[MAX_APPS]={0};
#if defined(_WIN32)
    HICON new_hicons[MAX_APPS]={0};
    int wantSize = IconSize;
    if (wantSize < 16) wantSize = 16;
    if (wantSize > 48) wantSize = 48;
#endif
    for (size_t i=0;i<n;i++) {
        const char* p=NULL; APPS_LOCK(); if(i < s_apps.count) p=s_apps.items[i].path; APPS_UNLOCK();
        if(p) {
            new_icons[i]=fl_get_icon_path((char*)p);
            const char* iconSrc = new_icons[i] ? new_icons[i] : p;
#if defined(_WIN32)
            new_hicons[i]=win_load_icon_for_path(iconSrc, wantSize);
#endif
        }
    }
    APPS_LOCK();
    for(size_t i=0;i<s_app_icon_count;i++){ free(s_app_icons[i]); s_app_icons[i]=NULL; }
#if defined(_WIN32)
    for(size_t i=0;i<s_hicon_count;i++){ if(s_hicons[i]) DestroyIcon(s_hicons[i]); s_hicons[i]=NULL; }
    s_hicon_count=0;
#endif
    s_app_icon_count=0;
    for(size_t i=0;i<n;i++) s_app_icons[i]=new_icons[i];
    s_app_icon_count=n;
#if defined(_WIN32)
    for(size_t i=0;i<n;i++) s_hicons[i]=new_hicons[i];
    s_hicon_count=n;
#endif
    APPS_UNLOCK();
}

#if defined(_WIN32)
static const char* kAppCachePath = "data/app_cache_win.txt";
static bool load_app_cache_from_file(void) {
    FILE* f = fopen(kAppCachePath, "r");
    if (!f) return false;
    ListOfFiles tmp = {0};
    char line[1024];
    while (fgets(line, sizeof(line), f) && tmp.count < MAX_APPS) {
        line[strcspn(line, "\r\n")] = '\0';
        char* sep = strchr(line, '|');
        if (!sep) continue;
        *sep = '\0';
        const char* name = line;
        const char* path = sep+1;
        if (tmp.count >= tmp.capacity) {
            tmp.capacity = tmp.capacity==0?256:tmp.capacity*2;
            FileItem* ni = realloc(tmp.items, tmp.capacity*sizeof(FileItem));
            if (!ni) { fclose(f); if(tmp.items) free(tmp.items); return false; }
            tmp.items = ni;
        }
        strncpy(tmp.items[tmp.count].name, name, sizeof(tmp.items[tmp.count].name)-1);
        tmp.items[tmp.count].name[sizeof(tmp.items[tmp.count].name)-1]='\0';
        strncpy(tmp.items[tmp.count].path, path, sizeof(tmp.items[tmp.count].path)-1);
        tmp.items[tmp.count].path[sizeof(tmp.items[tmp.count].path)-1]='\0';
        tmp.count++;
    }
    fclose(f);
    if (tmp.count==0) { if(tmp.items) free(tmp.items); return false; }
    APPS_LOCK();
    if (s_apps.items) free(s_apps.items);
    s_apps = tmp;
    APPS_UNLOCK();
    rebuild_icon_cache();
    return true;
}
static void save_app_cache_to_file(void) {
    FILE* f = fopen(kAppCachePath, "w");
    if (!f) return;
    APPS_LOCK();
    for (size_t i=0;i<s_apps.count;i++) fprintf(f, "%s|%s\n", s_apps.items[i].name, s_apps.items[i].path);
    APPS_UNLOCK();
    fclose(f);
}
#endif
void initialize_app_cache(void) {
#if defined(_WIN32)
    if (s_apps.count==0 && load_app_cache_from_file()) return;
#endif
#if defined(_WIN32)
    char* allowed[] = {"lnk", "exe", "url", NULL};
#else
    char* allowed[] = {"desktop",NULL};
#endif
    ListOfFiles tmp = {0};
    for (int i = 0; i < (int)ARRAY_LEN(ListOfProgramPaths); i++) {
        if (!ListOfProgramPaths[i] || ListOfProgramPaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfProgramPaths[i], allowed, &tmp, (char**)ListOfShitToIgnore);
    }
    for (size_t i = 0; i < tmp.count; ) {
        bool skip = false;
        char display_name[256] = {0};
        bool has_display = false;
        if (strstr(tmp.items[i].path, ".desktop")) {
            bool nodisplay=false;
            if (fl_parse_desktop_file(tmp.items[i].path, display_name, sizeof(display_name), &nodisplay)) has_display=true;
            if (nodisplay) skip=true;
        }
        if (skip) {
            for (size_t k = i; k + 1 < tmp.count; k++) tmp.items[k] = tmp.items[k+1];
            tmp.count--;
            continue;
        }
        if (has_display && display_name[0]) {
            snprintf(tmp.items[i].name, sizeof(tmp.items[i].name), "%s", display_name);
        }
        i++;
    }
    for (size_t i = 0; i < tmp.count; i++) {
        for (size_t j = i + 1; j < tmp.count; ) {
            if (fl_string_match_insensitive(tmp.items[i].name, tmp.items[j].name)) {
                for (size_t k = j; k + 1 < tmp.count; k++) tmp.items[k] = tmp.items[k+1];
                tmp.count--;
            } else {
                j++;
            }
        }
    }
    
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (tmp.count >= MAX_APPS) break;
        
        bool found = false;
        for (size_t app_idx = 0; app_idx < tmp.count; app_idx++) {
            if (fl_string_match_insensitive(tmp.items[app_idx].path, SystemShortcuts[shortcut_idx].path)) {
                strncpy(tmp.items[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.items[app_idx].name) - 1);
                tmp.items[app_idx].name[sizeof(tmp.items[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (tmp.count >= tmp.capacity) {
                tmp.capacity = tmp.capacity == 0 ? 10 : tmp.capacity * 2;
                tmp.items = realloc(tmp.items, tmp.capacity * sizeof(FileItem));
            }
            strncpy(tmp.items[tmp.count].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.items[tmp.count].name) - 1);
            tmp.items[tmp.count].name[sizeof(tmp.items[tmp.count].name) - 1] = '\0';
            
            strncpy(tmp.items[tmp.count].path, SystemShortcuts[shortcut_idx].path, sizeof(tmp.items[tmp.count].path) - 1);
            tmp.items[tmp.count].path[sizeof(tmp.items[tmp.count].path) - 1] = '\0';
            
            tmp.count++;
        }
    }
#endif
    APPS_LOCK();
    if (s_apps.items) free(s_apps.items);
    s_apps = tmp;
    APPS_UNLOCK();
    rebuild_icon_cache();
}
void launch_app_by_name(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    APPS_LOCK();
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(display_name,s_apps.items[i].name)) {
            strncpy(path_copy, s_apps.items[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    APPS_UNLOCK();
    if (found) fl_launch_with_args(path_copy, NULL);
}

void launch_app_as_admin(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    APPS_LOCK();
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(s_apps.items[i].name, display_name)) {
            strncpy(path_copy, s_apps.items[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    APPS_UNLOCK();
    if (found) fl_launch_with_args_as_admin(path_copy, NULL);
}

bool applauncher_enter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_by_name(selection);
    return true;
}

bool applauncher_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_as_admin(selection);
    return true;
}

void applauncher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    size_t cnt = 0;
    APPS_LOCK(); cnt = s_apps.count; APPS_UNLOCK();
    if (cnt == 0) {
        PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, NULL, 0, NULL);
        return;
    }
    if (cnt > 2048) {
        const char** names = (const char**)malloc(cnt * sizeof(char*));
        if (!names) return;
        APPS_LOCK();
        for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.items[i].name;
        APPS_UNLOCK();
        PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, names, (int)cnt, NULL);
        free(names);
        return;
    }
    const char* names[2048];
    APPS_LOCK();
    for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.items[i].name;
    APPS_UNLOCK();
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, names, (int)cnt, NULL);
}
void applauncher_copy_callback(const char* selection,int true_index,int type) {
    char* text = (char*)selection;
    switch(type) {
      case 0: {
        int start = strlen(text);
        for (int i = start; i > 0 ; i--) {
          if (text[i] == '\\') {
            for (int j = i; j < start; ++j){
                text[j] = '\0';
            }
            break;
          }
        }
        break;
      }
      case 1: {
        char pc[512]={0}; bool ok=false;
        APPS_LOCK();
        if (true_index >= 0 && (size_t)true_index < s_apps.count) { strncpy(pc, s_apps.items[true_index].path, sizeof(pc)-1); ok=true; }
        APPS_UNLOCK();
        if (ok) { char* dup=_strdup(pc); if(dup){ fl_copy_to_clipboard(dup); free(dup); return; } }
        break;
      }
    }
    fl_copy_to_clipboard(text);
}

const char* applauncher_get_icon_for_item(const char* item_name) {
    const char* ret=NULL;
    APPS_LOCK();
    for (size_t i=0;i<s_apps.count && i < MAX_APPS;i++) if (fl_string_match_insensitive(s_apps.items[i].name, item_name)) {
        ret = s_app_icons[i]; break;
    }
    APPS_UNLOCK();
    return ret;
}

void applauncher_free_icon_path(const char* p) {
    (void)p;
}

int applauncher_get_icon_size_for_item(const char* item_name) {
    const char* p = applauncher_get_icon_for_item(item_name);
    return fl_get_icon_size(p);
}


#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "core/window_manager.h"
#include "platform/x11/drawing.h"
void applauncher_render(Display* dpy, Window* win, PromptState* state) {
    if (!state || !state->has_listbox) return;
    ListBoxInfo *lb = &state->listbox;
    if (!lb->populated_count) return;
    XWindowAttributes attrs; Visual* vis=NULL; Colormap cmap=0;
    if (XGetWindowAttributes(dpy, *win, &attrs)) { vis=attrs.visual; cmap=attrs.colormap; }
    else { vis=DefaultVisual(dpy, DefaultScreen(dpy)); cmap=DefaultColormap(dpy, DefaultScreen(dpy)); }
    XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
    if (!xd) return;
    Linux_DrawBackground(dpy, *win, state->width, state->height);
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    int y0=70; int itemH=FontSize+14;
    int visible=(state->height - y0 - 40)/itemH;
    if(visible<1) visible=1;
    if(visible>lb->populated_count) visible=lb->populated_count;
    if(lb->selected_index>=0){
        if(lb->selected_index < lb->view_offset+PaddingForScrolling) lb->view_offset=lb->selected_index-PaddingForScrolling;
        else if(lb->selected_index >= lb->view_offset+visible-PaddingForScrolling) lb->view_offset=lb->selected_index-visible+PaddingForScrolling+1;
    }
    if(lb->view_offset<0) lb->view_offset=0;
    if(lb->view_offset+visible>lb->populated_count) lb->view_offset=lb->populated_count-visible;
    if(lb->view_offset<0) lb->view_offset=0;
    XftColor bgc,fcoltext,fcolalternate,fcolrainbow;
    if(!EnableShaders || ShaderPreset==0) Linux_XftAllocColor(dpy,vis,cmap,rgb_to_hex(COLOR_BG),OpacityAlpha16(),&bgc);
    Linux_XftAllocColor(dpy,vis,cmap,rgb_to_hex(COLOR_TEXT),0xFFFF,&fcoltext);
    Linux_XftAllocColor(dpy,vis,cmap,rgb_to_hex(COLOR_ALTERNATE),0xFFFF,&fcolalternate);
    for(int i=0;i<visible;i++){
        int idx=lb->view_offset+i; int y=y0+i*itemH; bool sel=(idx==lb->selected_index);
        if(sel) Linux_DrawSelectionBackground(dpy,xd,vis,cmap,0,y,state->width,itemH,rgb_to_hex(COLOR_SELECTED_TEXT));
        else if(!EnableShaders || ShaderPreset==0) XftDrawRect(xd,&bgc,0,y,state->width,itemH);
        float _t=(lb&&lb->populated_count>1)?(float)idx/(float)(lb->populated_count-1):(float)idx/(float)GradientItems;
        Linux_XftAllocColor(dpy,vis,cmap,rgb_to_hex(GetRainbowColor(_t,0.55f,GradientBrightness)),0xFFFF,&fcolrainbow);
        XftColor *num_color=&fcoltext,*body_color=&fcoltext,*plain_color=&fcolrainbow;
        int text_x=45;
        if(RainbowText) num_color=body_color=plain_color=&fcolrainbow;
        else if(AlternatingText && idx%2==0) num_color=body_color=plain_color=&fcolalternate;
        const char* item_text = (idx>=0 && idx < lb->populated_count) ? lb->populated_cache[idx] : "";
        const char* icon_path = applauncher_get_icon_for_item(item_text);
        DrawListBoxItemText(dpy,xd,vis,cmap,font,num_color,body_color,plain_color,lb,(char*)icon_path,idx,y,text_x);
        XftColorFree(dpy,vis,cmap,&fcolrainbow);
    }
    if(!EnableShaders || ShaderPreset==0) XftColorFree(dpy,vis,cmap,&bgc);
    XftColorFree(dpy,vis,cmap,&fcoltext);
    XftColorFree(dpy,vis,cmap,&fcolalternate);
    Platform_DestroyFont(font);
    XftDrawDestroy(xd);
}
#else
#include <windows.h>
#include <shellapi.h>
#include "config.h"
void applauncher_render(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    if (!text || !rect) return;
    UI_RedrawBackground(hdc, rect, is_selected);
    int iconSize = IconSize;
    if (iconSize < 16) iconSize = 16;
    if (iconSize > 32) iconSize = 32;
    HICON hIcon = NULL;
    bool needDestroy = false;
    char iconPathBuf[MAX_PATH]={0};
    const char* iconPathForFallback=NULL;
    if (text) {
        APPS_LOCK();
        for (size_t i=0;i<s_apps.count && i < s_hicon_count;i++) if (fl_string_match_insensitive(s_apps.items[i].name, text)) { hIcon = s_hicons[i]; if(hIcon) { /* cached */ } else if(i < s_app_icon_count && s_app_icons[i]) { strncpy(iconPathBuf, s_app_icons[i], sizeof(iconPathBuf)-1); iconPathForFallback=iconPathBuf; } else if(s_apps.items[i].path[0]) { strncpy(iconPathBuf, s_apps.items[i].path, sizeof(iconPathBuf)-1); iconPathForFallback=iconPathBuf; } break; }
        // if not found in cache (maybe cache not built yet), try direct lookup via s_apps
        if(!hIcon && !iconPathForFallback) {
            for(size_t i=0;i<s_apps.count;i++) if(fl_string_match_insensitive(s_apps.items[i].name, text)) { strncpy(iconPathBuf, s_apps.items[i].path, sizeof(iconPathBuf)-1); iconPathForFallback=iconPathBuf; break; }
            if(!iconPathForFallback) {
                const char* p = applauncher_get_icon_for_item(text);
                if(p) { strncpy(iconPathBuf, p, sizeof(iconPathBuf)-1); iconPathForFallback=iconPathBuf; }
            }
        }
        APPS_UNLOCK();
        if(!hIcon && iconPathForFallback && iconPathForFallback[0]) {
            SHFILEINFOA shfi={0};
            if(SHGetFileInfoA(iconPathForFallback,0,&shfi,sizeof(shfi),SHGFI_ICON|SHGFI_LARGEICON)) { hIcon=shfi.hIcon; needDestroy=true; }
            else if(SHGetFileInfoA(iconPathForFallback,FILE_ATTRIBUTE_NORMAL,&shfi,sizeof(shfi),SHGFI_ICON|SHGFI_LARGEICON|SHGFI_USEFILEATTRIBUTES)) { hIcon=shfi.hIcon; needDestroy=true; }
            else if(SHGetFileInfoA(iconPathForFallback,0,&shfi,sizeof(shfi),SHGFI_ICON|SHGFI_SMALLICON)) { hIcon=shfi.hIcon; needDestroy=true; }
            else if(SHGetFileInfoA(iconPathForFallback,FILE_ATTRIBUTE_NORMAL,&shfi,sizeof(shfi),SHGFI_ICON|SHGFI_SMALLICON|SHGFI_USEFILEATTRIBUTES)) { hIcon=shfi.hIcon; needDestroy=true; }
        }
    }
    RECT textRect = *rect;
    if (hIcon) {
        int y = rect->top + ((rect->bottom - rect->top) - iconSize) / 2;
        int oldMode = SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        DrawIconEx(hdc, rect->left + 6, y, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        SetStretchBltMode(hdc, oldMode);
        if(needDestroy) DestroyIcon(hIcon);
        textRect.left += iconSize + 12;
    } else {
        textRect.left += 10;
    }
    HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COLOR_TEXT);
    DrawTextA(hdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, hOldFont);
}
#endif

void TriggerAppLauncher(void) {
    for (size_t i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
        fl_on_dir_changed(ListOfProgramPaths[i], initialize_app_cache);
    }
    int _cnt=0; APPS_LOCK(); _cnt=(int)s_apps.count; APPS_UNLOCK();
    int ih = IconSize + 8;
    if (ih < 32) ih = 32;
    if (ih > 48) ih = 48;
    PROMPT(.title = "🚀 AppLauncher", .items = NULL, .count = _cnt,
           .height = 920, .width = 650, .item_height = ih,
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback,
           .render = applauncher_render);
}

#if defined(_WIN32)
#include <windows.h>
static DWORD WINAPI app_launcher_bg_init(LPVOID p) {
    (void)p;
    initialize_app_cache();
    return 0;
}
FLINT_FEATURE_INIT {
    HANDLE h = CreateThread(NULL, 0, app_launcher_bg_init, NULL, 0, NULL);
    if (h) CloseHandle(h);
    fl_register_hotkey("Alt+E", TriggerAppLauncher);
    fl_register_feature("App Launcher", TriggerAppLauncher);
}
#else
FLINT_FEATURE_INIT {
    initialize_app_cache();
    fl_register_hotkey("Alt+E", TriggerAppLauncher);
    fl_register_feature("App Launcher", TriggerAppLauncher);
}
#endif








