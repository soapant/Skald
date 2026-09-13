#include "core/flapi.h"

const char* BASE_SCREENSHOT_FOLDER_PATH = "{PICTURES}/Screenshots/";
static long img_x2 = 0, img_y2 = 0;
static long img_x1 = 0, img_y1 = 0;
static int img_region_step = 0;

static void TriggerScreenshot(WindowHandle hPopupWnd,bool open_screenshot) {
    const char* name_prompt = "Enter image name...";
    char* raw_name = SIMPLE_PROMPT(.prompt = name_prompt, .title = "📷 Screenshot");

    if (raw_name != NULL && strlen(raw_name) > 0) {
        char final_file_path[512];
        char expanded_folder[MAX_PATH];
        if (!Platform_ExpandPath(BASE_SCREENSHOT_FOLDER_PATH, expanded_folder, sizeof(expanded_folder))) return;
        snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", expanded_folder, raw_name);
        
        Platform_SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
        printf("[SCREENSHOT] Saved: %s\n", final_file_path);
        if (open_screenshot) {
#if defined(_WIN32)
          char expanded_editor[MAX_PATH];
          if (!Platform_ExpandPath(ImageEditorPath, expanded_editor, sizeof(expanded_editor))) return;
          fl_launch_with_args(expanded_editor, final_file_path);
          printf("[SCREENSHOT] Opening in editor: %s\n", final_file_path);
#else
          fl_launch_with_args(final_file_path, NULL);
          printf("[SCREENSHOT] Opening: %s\n", final_file_path);
#endif
        }
    } else {
        printf("[SCREENSHOT] Aborted (no name provided)\n");
    }
    
    img_region_step = 0;
}

void TriggerSaveScreenshot(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[SCREENSHOT] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        TriggerScreenshot(FlintContext_Get()->window.hPopupWnd, false);
    }
#else

#endif
}

void TriggerOpenScreenshot(void) {
#if defined(_WIN32)
    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) return;
    
    extern int img_region_step;
    extern long img_x1, img_y1, img_x2, img_y2;
    
    if (img_region_step == 0) {
        img_x1 = cursorPos.x;
        img_y1 = cursorPos.y;
        img_region_step = 1;
        printf("[SCREENSHOT] Top-Left: %ld,%ld\n", img_x1, img_y1);
    } else {
        img_x2 = cursorPos.x;
        img_y2 = cursorPos.y;
        img_region_step = 0;
        TriggerScreenshot(FlintContext_Get()->window.hPopupWnd, true);
    }
#else

#endif
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Ctrl+A", TriggerSaveScreenshot);
    fl_register_hotkey("Alt+Shift+A", TriggerOpenScreenshot);
}
