#ifndef MUSIC_PLAYER_H
#define  MUSIC_PLAYER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"
#include "file_search.h"
#include "app_launcher.h"

extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;


static bool HandleMusicPlayer(int vk_code, int modifiers, const char* current_title) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return false;
    }
    for (int i = 0; i < MUSIC_PLAYER_COUNT; i++) {
        const MusicPlayerBinding* binding = &G_MUSIC_PLAYERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            HWND playerHwnd = FindWindow(NULL, "Music Player");
            if (binding->code != 69) {
              if (playerHwnd) {
                  PostMessage(playerHwnd, WM_USER + 1, binding->code, 0);
                  return true;
              } else {
                  printf("[Music Player] No matching window found.\n");
                  return false;
              }
            } else {
          ClearFileList();
          GetFilesRecursive("D:\\Music");

          const char* prompt[MAX_FILES];
          for (int i = 0; i < GetFileCount(); i++) {
              prompt[i] = G_FILES[i].name;
          }

          char* input = PromptQuickSwitcher(prompt, GetFileCount(), 220, 350);
              
          if (input != NULL && strlen(input) > 0) {
              char* result = SearchFileInDirectory("D:\\Music",input);
              if (result) {
                if (playerHwnd) {
                  COPYDATASTRUCT cds;
                  cds.dwData = 0;                          
                  cds.cbData = (DWORD)(strlen(result) + 1);
                  cds.lpData = (PVOID)result;               
                  SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
                }else {
                  char music_player_path[MAX_PATH] = {0};
                  for (int i = 0; i < G_APP_COUNT; i++) {
                      if (StrStrIA(G_APPS[i].name, "music_player") != NULL) {
                          strncpy(music_player_path, G_APPS[i].path, sizeof(music_player_path) - 1);
                          break;
                      }
                  }
                  char quoted_result[MAX_PATH + 3] = {0};
                  snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
                  launch_exe_with_args(music_player_path, quoted_result);
                }
              }
            }
          }
        }
    }
    return false;
}

#endif
