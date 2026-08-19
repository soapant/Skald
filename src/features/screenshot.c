#include "common.h"
#include "features/screenshot.h"
#include "core/common_ui.h"

static void TriggerScreenshot(WindowHandle hPopupWnd,bool open_screenshot) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char* name_prompt = "Enter image name...";
    char* raw_name = Platform_SimplePromptUser(name_prompt,"📷 Screenshot",920, 650,32);

    if (raw_name != NULL && strlen(raw_name) > 0) {
        char final_file_path[512];
        snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", BASE_SCREENSHOT_FOLDER_PATH, raw_name);
        
        Platform_SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
        printf("[SCREENSHOT] Saved: %s\n", final_file_path);
        if (open_screenshot) {
          char open_cmd[1024];
#if defined(_WIN32)
          snprintf(open_cmd, sizeof(open_cmd), "start %s \"%s\"", ImageEditorPath, final_file_path);
#else
          snprintf(open_cmd, sizeof(open_cmd), "xdg-open \"%s\"", final_file_path);
#endif
          system(open_cmd);
          printf("[SCREENSHOT] Opening in editor: %s\n", open_cmd);
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
        TriggerScreenshot(hPopupWnd, false);
    }
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
        TriggerScreenshot(hPopupWnd, true);
    }
#endif
}