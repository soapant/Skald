#include "core/flapi.h"

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
  "yazi",
  "obj",
  "build",
};

const char* ListOfProgramPaths[] = {
#if defined(_WIN32)
    "{APPDATA}/Microsoft/Windows/Start Menu",
    "{PROGRAMDATA}/Microsoft/Windows/Start Menu/Programs",
    "{DEV}",
    "{MISC}",
    "{SystemRoot}/System32/",             
    "shell:AppsFolder",
#else
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/home/diggy/.local/share/applications",
    "/usr/bin",
    "/home/diggy/.local/bin",
    "/home/diggy/Programming",
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

void initialize_app_cache(void) {
#if defined(_WIN32)
    char* allowed[] = {"lnk", "exe", "url", NULL};
#else
    char* allowed[] = {"desktop",NULL};
#endif
    s_apps.count = 0;
    for (int i = 0; i < (int)ARRAY_LEN(ListOfProgramPaths); i++) {
        if (!ListOfProgramPaths[i] || ListOfProgramPaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfProgramPaths[i], allowed, &s_apps, (char**)ListOfShitToIgnore);
    }
    for(size_t i=0;i<s_apps.count;i++) if(strstr(s_apps.items[i].path,".desktop")){
      FILE*f=fopen(s_apps.items[i].path,"r"); char line[512];
      while(fgets(line,sizeof line,f)) if(strncmp(line,"Name=",5)==0){
        line[strcspn(line,"\r\n")]='\0';
        strncpy(s_apps.items[i].name, line+5, sizeof(s_apps.items[i].name)-1);
        break;
      }
      fclose(f);
    }
    
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (s_apps.count >= MAX_APPS) break;
        
        bool found = false;
        for (size_t app_idx = 0; app_idx < s_apps.count; app_idx++) {
            if (fl_string_match_insensitive(s_apps.items[app_idx].path, SystemShortcuts[shortcut_idx].path)) {
                strncpy(s_apps.items[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[app_idx].name) - 1);
                s_apps.items[app_idx].name[sizeof(s_apps.items[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (s_apps.count >= s_apps.capacity) {
                s_apps.capacity = s_apps.capacity == 0 ? 10 : s_apps.capacity * 2;
                s_apps.items = realloc(s_apps.items, s_apps.capacity * sizeof(FileItem));
            }
            strncpy(s_apps.items[s_apps.count].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[s_apps.count].name) - 1);
            s_apps.items[s_apps.count].name[sizeof(s_apps.items[s_apps.count].name) - 1] = '\0';
            
            strncpy(s_apps.items[s_apps.count].path, SystemShortcuts[shortcut_idx].path, sizeof(s_apps.items[s_apps.count].path) - 1);
            s_apps.items[s_apps.count].path[sizeof(s_apps.items[s_apps.count].path) - 1] = '\0';
            
            s_apps.count++;
        }
    }
#endif
}
void launch_app_by_name(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(display_name,s_apps.items[i].name)) {
            fl_launch_with_args(s_apps.items[i].path, NULL);
            return;
        }
    }
}

void launch_app_as_admin(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(s_apps.items[i].name, display_name)) {
            fl_launch_with_args_as_admin(s_apps.items[i].path, NULL);
            return;
        }
    }
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
    const char** names = (const char**)malloc(s_apps.count * sizeof(char*));
    for (size_t i = 0; i < s_apps.count; i++) {
        names[i] = s_apps.items[i].name;
    }
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, names, (int)s_apps.count, NULL);
    free(names);
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
        if (true_index >= 0 && (size_t)true_index < s_apps.count) {
            text = s_apps.items[true_index].path;
        }
        break;
      }
    }
    fl_copy_to_clipboard(text);
}

const char* applauncher_get_icon_for_item(const char* item_name) {
    for (size_t i=0;i<s_apps.count;i++) if (fl_string_match_insensitive(s_apps.items[i].name, item_name)) {
        char* p = fl_get_icon_path(s_apps.items[i].path);
        return p;
    }
    return NULL;
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
        if(!icon_path) icon_path=NULL;
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
void applauncher_render(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    if (!text || !rect) return;
    UI_RedrawBackground(hdc, rect, is_selected);
    RECT textRect = *rect;
    textRect.left += 10;
    HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextA(hdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, hOldFont);
}
#endif

void TriggerAppLauncher(void) {
    const char** initial_items = (const char**)malloc(s_apps.count * sizeof(char*));
    if (!initial_items) return;
    
    for (int i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
        fl_on_dir_changed(ListOfProgramPaths[i], initialize_app_cache);
    }

    PROMPT(.title = "🚀 AppLauncher", .items = initial_items, .count = (int)s_apps.count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback,
           .render = applauncher_render);

    free(initial_items);
}

FLINT_FEATURE_INIT {
    initialize_app_cache();
    fl_register_hotkey("Alt+E", TriggerAppLauncher);
    fl_register_feature("App Launcher", TriggerAppLauncher);
}

