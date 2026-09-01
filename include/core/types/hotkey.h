#ifndef TYPES_HOTKEY_H
#define TYPES_HOTKEY_H

#include "core/types/window.h"

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* app_name;  
} AppHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* misc_path; 
} MiscHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char* terminal_command;
    bool open_in_terminal;
} TerminalCommandHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* QuickLinkQuery; 
} QuickLinkHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;      
    int blacklist_count;
    const char** window_titles;  
    int window_count;
} FocusSwitcherBinding;

typedef struct {
    int vk_code;
    int modifiers;
    int code;
} MusicPlayerBinding;

typedef struct {
    int vk_code;
    int modifiers;
    int code;
} VideoViewerBinding;

typedef struct {
    int vk_code;
    int modifiers;
    const char** window_titles;  
    int window_count;
} BulkKillProcessBinding;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* text;  
} TextSnippet;

typedef struct {
   char* alias;
   char* text;
   char* feature;
} AliasEntry;

#endif
