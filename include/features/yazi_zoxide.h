#ifndef YAZI_ZOXIDE_H
#define YAZI_ZOXIDE_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"
#include "core/platform.h"

const char* YaziZoxideFilePath = "data/YaziZoxide.txt";
MappedFile YaziZoxideFileData = {0};
bool YaziZoxideFileInit = false;
MappedFile* GetYaziZoxideFile(void) {
    if (!YaziZoxideFileInit) {
#if !defined(_WIN32)
        YaziZoxideFileData.fd = -1;
#endif
        Platform_FileInit(YaziZoxideFilePath, &YaziZoxideFileData);
        YaziZoxideFileInit = true;
    }
    return &YaziZoxideFileData;
}
#define YaziZoxideFile GetYaziZoxideFile()



char** g_cached_yazi_commands = NULL;
int g_cached_yazi_count = 0;

void RebuildYaziCache(void) {
    ZoxideHelper_RebuildCache(YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count);
}


void yazi_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count);
}


void yazi_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = fl_get_zoxide_match(selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
          fl_launch_terminal_at_path(path, cmd_args);
          ZoxideHelper_AppendNewEntry(YaziZoxideFile, selection, lb, &g_cached_yazi_commands, &g_cached_yazi_count);
          free(path);
      }
}

void yazi_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count, yazi_refresh_callback);
}

void TriggerYaziZoxide(void) {
    if (!g_cached_yazi_commands) RebuildYaziCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_yazi_commands, &g_cached_yazi_count, NULL);
    PROMPT(.title = "📂 Yazi", .items = list, .count = g_cached_yazi_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = yazi_refresh_callback,
           .enter = yazi_enter_callback,
           .edit = yazi_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

#endif
