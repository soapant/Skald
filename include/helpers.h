#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "core/modal.h"
#include "core/types.h"
#include "core/hashmap.h"
#include "config.h"
#include "platform/platform.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

typedef void (*HotkeyHandler)(void);
typedef bool (*HotkeyCondition)(void);
extern char* SelectionData[2048];
extern int SelectionDataCount;

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    HotkeyHandler handler;
    const char* name;
    HotkeyCondition is_enabled;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);
extern EditorMode g_last_mode;
bool AlwaysEnabled(void);
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                   int pressed_vk, int modifiers,
                   const char* current_title);


typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

typedef struct {
    char* item_name;
    int item_index;
    char* alias_name; 
} AliasInfo;

typedef struct {
    AliasInfo matches[1024];
    int count;
} AliasMatchList;

typedef struct {
    char* flag;
    int index;
} FoundFlag;

int ParseTimeToSeconds(const char* input);
typedef void (*ActionFn)(const char* parameter);
typedef bool (*ConditionFn)(const char* parameter);
typedef void (*TimerFn)(const char* parameter);
typedef struct { char* str; ActionFn    func; } ActionDef;
typedef struct { char* str; ConditionFn func; } ConditionDef;
typedef struct { char* str; TimerFn     func; } TimerDef;
typedef struct {
    char* name;
    ConditionFn func;
} ConditionParamDef;

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches);
char* GetFlagParameter(const char* text, int flag_index);
char* GetCleanText(const char* text);
bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count);

int UI_LookupTrueIndex(const char* name);
void SaveFeatureHistory(char* string);
int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text);
void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text);
int ReadMdFile(char lines[100][512], const char* path);
void StripCheckboxes(char* str);
void WriteMdFile(char lines[100][512], int count, const char* path);
extern HashMap* history_indices;
bool GetFromHistory(ListBoxInfo* lb);
int convert_string_to_int(const char *str);
void HandleCommandExecution(ListBoxInfo* lb, char* string);
const char* get_alias_for_item(const char* item);
unsigned long rgb_to_hex(unsigned long win_color);
#if !defined(_WIN32)
void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY,char* centered_to,int xOffset);
bool CursorBack(size_t* cursor, size_t text_len);
bool CursorForward(size_t* cursor, size_t text_len);
#endif

#endif
