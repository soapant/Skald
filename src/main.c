#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "helpers.h"
#include "core/feature_data.h"
#include "core/flapi.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#endif
#if !defined(_WIN32)
#include "platform/x11/display.h"
#include <fontconfig/fontconfig.h>
#endif
#if defined(_WIN32)
// notification getter decls (src/features/notification_getter.c - no header)
extern const wchar_t* HttpUrlForNotis;
extern void StartNtfyDaemon(const wchar_t* topic, void (*callback)(const char*));
#endif


// Window handles and feature globals now in FlintContext (src/core/context.c)


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
  FlintContext_Get()->g_main_thread_id = GetCurrentThreadId();
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
#if defined(_WIN32)
  StartNtfyDaemon(HttpUrlForNotis,OInNotificationRecieved);
#endif
  FlintContext_Get()->feature_history = HashMap_Create(32);
  InitFeatureDataSystem();
  Platform_InitUI();

  FeatureLoader_Init();
  
  WindowManager* wm = WindowManager_Create();
  if (!wm) {
      fprintf(stderr, "[SYSTEM] Failed to create WindowManager\n");
      return 1;
  }
  
  WindowManager_Run(wm);
  WindowManager_Destroy(wm);
  
  FeatureLoader_Shutdown();
  Platform_ShutdownUI();
#if !defined(_WIN32)
  cleanup_display();
#endif

#if defined(_WIN32)
  if (FlintContext_Get()->g_hNumFont) {
    DeleteObject(FlintContext_Get()->g_hNumFont);
    FlintContext_Get()->g_hNumFont = NULL;
  }
  if (FlintContext_Get()->g_hMainFont) {
    DeleteObject(FlintContext_Get()->g_hMainFont);
    FlintContext_Get()->g_hMainFont = NULL;
  }
#endif
  CleanupFeatureDataSystem();

  FlintContext* _ctx = FlintContext_Get();
  if (_ctx->feature_history) {
    for (int i = 0; i < _ctx->feature_history->size; i++) {
        HashMapEntry* entry = _ctx->feature_history->buckets[i];
        while (entry) {
            char** list = (char**)entry->value;
            if (list) {
                for (int k = 0; list[k]; k++) free(list[k]);
                free(list);
            }
            entry = entry->next;
        }
    }
    HashMap_Destroy(_ctx->feature_history);
    _ctx->feature_history = NULL;
  }
  if (_ctx->history_indices) {
    for (int i = 0; i < _ctx->history_indices->size; i++) {
        HashMapEntry* entry = _ctx->history_indices->buckets[i];
        while (entry) {
            free(entry->value);
            entry = entry->next;
        }
    }
    HashMap_Destroy(_ctx->history_indices);
    _ctx->history_indices = NULL;
  }
  if (_ctx->g_item_index_map) {
    HashMap_Destroy(_ctx->g_item_index_map);
    _ctx->g_item_index_map = NULL;
  }
#if !defined(_WIN32)
  extern void CleanupPerFeatureVimMap(void);
  CleanupPerFeatureVimMap();
  FcFini();
#endif

  return 0;
}
