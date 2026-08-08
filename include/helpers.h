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

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,COLORREF color);
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

char* QueryZoxidePath(const char* query);

bool ShellExecuteWithVerb(const char* path, const char* verb, bool async);
bool LaunchApp(const char* path);
bool LaunchAppAsAdmin(const char* path);
bool IsCtrlPressed(void);

typedef struct {
    size_t index;
    char original;
} NewlineInfo;

typedef struct {
  char* item_name;
  int item_index;
  char* alias_name; 
} AliasInfo;

typedef struct {
    AliasInfo matches[1024];
    int count;
} AliasMatchList;

int FileInit(const char *filePath, MappedFile *mf, size_t initialPadding);
int FileAppend(MappedFile *mf, const char *text);
void FileClose(MappedFile *mf);
Line* FileGetLines(MappedFile *mf, size_t *outLineCount);
char** FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount);
void CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount);
void SanitizeFile(const char *filePath);

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

void StartUniversalTimer(void);
void ResetUniversalTimer(void);
void ModifyUniversalTimer(double seconds_to_add);
double GetUniversalTimeSeconds(void);

int CopyToClipboard(const char* text);
int UI_LookupTrueIndex(const char* name);
void SaveFeatureHistory(char* string);
int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text);
int ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text);
void WriteToCacheIndex(int index, const char* text);
int ReadMdFile(char lines[100][512], const char* path);
void StripCheckboxes(char* str);
void WriteMdFile(char lines[100][512], int count, const char* path);
extern HashMap* history_indices;
bool GetFromHistory(void);
int convert_string_to_int(const char *str);
void HandleCommandExecution(char* string);
const char* get_alias_for_item(const char* item);

#endif
