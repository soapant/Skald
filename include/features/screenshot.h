#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include "common.h"
#include "core/flapi.h"
#include "core/platform.h"
#include "core/common_ui.h"

const char* BASE_SCREENSHOT_FOLDER_PATH = "{PICTURES}/Screenshots/";
static int region_step = 0;
static long saved_x1 = 0;
static long saved_y1 = 0;
static long img_x2 = 0, img_y2 = 0;
static long img_x1 = 0, img_y1 = 0;
static int img_region_step = 0;

static void TriggerScreenshot(WindowHandle hPopupWnd,bool open_screenshot) {
    const char* name_prompt = "Enter image name...";
    char* raw_name = SIMPLE_PROMPT(.prompt = name_prompt, .title = "📷 Screenshot",
                                   .height = 920, .width = 650, .item_height = 32);

    if (raw_name != NULL && strlen(raw_name) > 0) {
        char final_file_path[512];
        char expanded_folder[MAX_PATH];
        if (!Platform_ExpandPath(BASE_SCREENSHOT_FOLDER_PATH, expanded_folder, sizeof(expanded_folder))) return;
        snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", expanded_folder, raw_name);
        
        Platform_SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
        printf("[SCREENSHOT] Saved: %s\n", final_file_path);
        if (open_screenshot) {
          char open_cmd[1024];
#if defined(_WIN32)
          char expanded_editor[MAX_PATH];
          if (!Platform_ExpandPath(ImageEditorPath, expanded_editor, sizeof(expanded_editor))) return;
          snprintf(open_cmd, sizeof(open_cmd), "start %s \"%s\"", expanded_editor, final_file_path);
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
#else
    // Not Implemented
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
#else
    // Not Implemented
#endif
}

static inline void screenshot_init(void) {
    fl_register_hotkey("Alt+Ctrl+A", TriggerSaveScreenshot);
    fl_register_hotkey("Alt+Shift+A", TriggerOpenScreenshot);
}

#endif
