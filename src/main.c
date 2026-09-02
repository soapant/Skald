#include "common.h"
#include "core/types.h"
#include "helpers.h"
#include "core/feature_data.h"
#include "core/flapi.h"
#include "features/app_launcher.h"
#include "features/todo.h"
#include "core/window_manager.h"
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#endif
#if !defined(_WIN32)
#include "x11_display.h"
#include <fontconfig/fontconfig.h>
#endif
#if defined(_WIN32)
#include "features/notification_getter.h"
#endif


WindowHandle hGlobalTrayWnd = WH_NULL;
WindowHandle hPopupWnd = WH_NULL;
WindowHandle hEditControl = WH_NULL;
WindowHandle hListBox = WH_NULL;
WindowHandle hDisplayMessage = WH_NULL;
WindowHandle hSettings = WH_NULL;
WindowHandle g_hEditPopup = WH_NULL;
WindowHandle g_hSearchPopup = WH_NULL;
EditPopupData* g_EditData = NULL;
bool setting_complete = false;
WindowHandle g_ActiveEditControl = WH_NULL;
int g_timeout = 1;
uint32_t g_main_thread_id = 0;
bool g_waiting_for_key = false;
int g_key_pressed = 0;
WinTimer g_timer;
#if defined(_WIN32)
HANDLE g_key_event = NULL;
#endif
#if defined(_WIN32)
HFONT g_hNumFont = NULL;
HFONT g_hMainFont = NULL;
#endif
HashMap* feature_history = NULL;
HashMap* g_item_index_map = NULL;


void OInNotificationRecieved(const char* text) {
  SubjectiveMessageDisplay(text,140,TimeOutForMessages,COLOR_TEXT);
}

int main() {
#if !defined(_WIN32)
  {
    int lock_fd = open("/tmp/flint.lock", O_RDWR | O_CREAT, 0666);
    if (lock_fd < 0) {
      fprintf(stderr, "[SYSTEM] Cannot open singleton lock /tmp/flint.lock (%s).\n"
                      "If it is owned by another user, remove it: sudo rm /tmp/flint.lock\n",
              strerror(errno));
      return 1;
    }
    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
      fprintf(stderr, "[SYSTEM] Another instance is already running. Exiting.\n");
      close(lock_fd);
      return 1;
    }
    fcntl(lock_fd, F_SETFD, FD_CLOEXEC);
  }
#endif
#if defined(_WIN32)
  {
    HANDLE mtx = CreateMutexA(NULL, TRUE, "flint_single_instance_mutex");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
      fprintf(stderr, "[SYSTEM] Another instance is already running. Exiting.\n");
      return 1;
    }
  }
#endif
#if !defined(_WIN32)
  if (!init_display()) {
    fprintf(stderr, "[SYSTEM] Continuing without X11 display (headless mode)\n");
  }
#endif
#if defined(_WIN32)
  g_main_thread_id = GetCurrentThreadId();
  {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
      char* slash = strrchr(buf, '\\');
      if (slash) *slash = '\0';
      for (int depth = 0; depth < 4; ++depth) {
        char probe[MAX_PATH];
        snprintf(probe, sizeof(probe), "%s\\data", buf);
        DWORD attr = GetFileAttributesA(probe);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
          SetCurrentDirectoryA(buf);
          break;
        }
        slash = strrchr(buf, '\\');
        if (!slash) break;
        *slash = '\0';
      }
    }
  }
#endif

  Platform_StartUniversalTimer();
  Platform_InitializeTrayIcon();
  initialize_app_cache();
#if defined(_WIN32)
  StartNtfyDaemon(HttpUrlForNotis,OInNotificationRecieved);
#endif
  feature_history = HashMap_Create(32);
  InitFeatureDataSystem();
  Platform_InitUI();

  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    G_FILE_ENTRIES[i].mf = (MappedFile*)calloc(1, sizeof(MappedFile));
    if (G_FILE_ENTRIES[i].mf != NULL) {
#if !defined(_WIN32)
        G_FILE_ENTRIES[i].mf->fd = -1;
#endif
    }
  }
#if !defined(_WIN32)
  FcInit();
#endif
  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    if (!G_FILE_ENTRIES[i].mf) continue;
    const char* path = G_FILE_ENTRIES[i].path;


    if (!Platform_FileInit(path, G_FILE_ENTRIES[i].mf)) {
      printf("[SYSTEM] Failed to open %s. Creating new.\n", G_FILE_ENTRIES[i].name);
#if defined(_WIN32)
      HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        Platform_FileInit(path, G_FILE_ENTRIES[i].mf); 
      }
#endif
    }
  }

  printf("[SYSTEM] Low-Level Hotkey Services Running. GDI+ Online.\n");
  fl_init_all_hotkeys();
  
  WindowManager* wm = WindowManager_Create();
  if (!wm) {
      fprintf(stderr, "[SYSTEM] Failed to create WindowManager\n");
      return 1;
  }
  
  WindowManager_Run(wm);
  WindowManager_Destroy(wm);
  
  Platform_ShutdownUI();
#if !defined(_WIN32)
  cleanup_display();
#endif

  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    if (G_FILE_ENTRIES[i].mf != NULL) {
      Platform_FileClose(G_FILE_ENTRIES[i].mf);
      free(G_FILE_ENTRIES[i].mf);
      G_FILE_ENTRIES[i].mf = NULL;
    }
  }
#if defined(_WIN32)
  if (g_hNumFont) {
    DeleteObject(g_hNumFont);
    g_hNumFont = NULL;
  }
  if (g_hMainFont) {
    DeleteObject(g_hMainFont);
    g_hMainFont = NULL;
  }
#endif
  CleanupFeatureDataSystem();

  if (feature_history) {
    for (int i = 0; i < feature_history->size; i++) {
        HashMapEntry* entry = feature_history->buckets[i];
        while (entry) {
            char** list = (char**)entry->value;
            if (list) {
                for (int k = 0; list[k]; k++) free(list[k]);
                free(list);
            }
            entry = entry->next;
        }
    }
    HashMap_Destroy(feature_history);
    feature_history = NULL;
  }
  if (history_indices) {
    for (int i = 0; i < history_indices->size; i++) {
        HashMapEntry* entry = history_indices->buckets[i];
        while (entry) {
            free(entry->value);
            entry = entry->next;
        }
    }
    HashMap_Destroy(history_indices);
    history_indices = NULL;
  }
  if (g_item_index_map) {
    HashMap_Destroy(g_item_index_map);
    g_item_index_map = NULL;
  }
#if !defined(_WIN32)
  extern void CleanupPerFeatureVimMap(void);
  CleanupPerFeatureVimMap();
  FcFini();
#endif

  return 0;
}
