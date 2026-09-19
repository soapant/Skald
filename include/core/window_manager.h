#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "core/types.h"
#include "core/context.h"

typedef struct Popup Popup;
typedef struct WindowManager WindowManager;

typedef void (*EditPopupConfirmCallback)(const char* text, void* user_data);

typedef struct {
    void (*on_show)(Popup* self);
    void (*on_hide)(Popup* self);
    void (*on_key)(Popup* self, int vk_code, bool pressed, int modifiers);
    void (*on_render)(Popup* self);
    void (*on_focus_lost)(Popup* self);
    void (*destroy)(Popup* self);
} PopupVTable;

typedef enum { POPUP_PROMPT,
  POPUP_SIMPLE, 
  POPUP_EDIT, 
  POPUP_MESSAGE, 
  POPUP_SEARCH } PopupType;

struct Popup {
    const PopupVTable* vtable;
    WindowHandle handle;
    WindowManager* manager;
    bool visible;
    void* user_data;
    PopupType type;
};

struct WindowManager {
    WindowHandle main_window;
    Popup** popups;
    int popup_count;
    int popup_capacity;
    bool running;
    FlintContext* ctx;
    
    #if !defined(_WIN32)
    Display* display;
    #else
    void* display;
    #endif
    bool watcher_stop;
    void* watcher_thread;
    bool watcher_running;
};

typedef struct {
    Popup base;
    char* text;
    int text_capacity;
    int cursor;
    char* title;
    char* align;
    EditPopupConfirmCallback on_confirm;
    void* user_data;
    int x, y, w, h;
    bool confirmed;
    int saved_mode;
} WMEditPopupData;

WindowManager* WindowManager_Create(void);
void WindowManager_Destroy(WindowManager* wm);
int WindowManager_Run(WindowManager* wm);
void WindowManager_Quit(WindowManager* wm);
WindowManager* WindowManager_Get(void);

Popup* WindowManager_PushPopup(WindowManager* wm, Popup* popup);
void WindowManager_PopPopup(WindowManager* wm);
Popup* WindowManager_GetTopPopup(WindowManager* wm);

char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec);
char* WindowManager_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec);


Popup* EditPopup_Create(WindowManager* wm, const char* initial_text,
                        const char* title, int x, int y, int w, int h, const char* align,
                        EditPopupConfirmCallback on_confirm, void* user_data);
Popup* PromptPopup_Create(WindowManager* wm, const PromptSpec* spec);
Popup* SimplePromptPopup_Create(WindowManager* wm, const SimplePromptSpec* spec);
Popup* SearchPopup_Create(WindowManager* wm, const char* title,
                          int x, int y, int w, int h,
                          EditPopupConfirmCallback on_confirm, void* user_data);
Popup* MessagePopup_Create(WindowManager* wm, const char* message, int timeout_ms);

void WindowManager_GetMainMonitorGeometry(int *x, int *y, int *w, int *h);
void WindowManager_GetCenteredPos(int win_w, int win_h, int *out_x, int *out_y);

#endif