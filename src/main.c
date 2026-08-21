#include "common.h"
#include "core/types.h"
#include "helpers.h"
#include "core/feature_data.h"
#include "features/app_launcher.h"
#include "features/todo.h"
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#endif
#if !defined(_WIN32)
#include <X11/Xlib.h>
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
char output_buffer[256];
bool selection_complete = false;
bool setting_complete = false;
int region_step = 0;
long saved_x1 = 0;
long saved_y1 = 0;
long img_x2 = 0, img_y2 = 0;
RefreshCallback g_refresh_callback = NULL;
EnterCallback g_enter_callback = NULL;
CtrlEnterCallback g_ctrl_enter_callback = NULL;
TabCallback g_tab_callback = NULL;
HelpCallback g_help_callback = NULL;
CopyCallback g_copy_callback = NULL;
RenderCallback g_render_callback = NULL;
AliasCallback g_alias_callback = NULL;
EditCallback g_edit_callback = NULL;
FilterCallback g_filter_callback = NULL;
WindowHandle g_ActiveEditControl = WH_NULL;
char* g_current_feature = NULL;
int g_timeout = 1;
uint32_t g_main_thread_id = 0;
bool g_waiting_for_key = false;
int g_key_pressed = 0;
WinTimer g_timer;
#if defined(_WIN32)
HANDLE g_key_event = NULL;
#endif
const char** g_initial_items = NULL;
int g_initial_count = 0;
const char* g_title = NULL;
int img_region_step = 0;
long img_x1 = 0, img_y1 = 0;
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
  XInitThreads();
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
    Platform_SanitizeFile(G_FILE_ENTRIES[i].path);
  }

  for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
    G_FILE_ENTRIES[i].mf = (MappedFile*)calloc(1, sizeof(MappedFile));
    if (G_FILE_ENTRIES[i].mf != NULL) {
#if !defined(_WIN32)
        G_FILE_ENTRIES[i].mf->fd = -1;
#endif
    }
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
  Platform_RunHotKeyMessageLoop();

  Platform_ShutdownUI();

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

  return 0;
}
