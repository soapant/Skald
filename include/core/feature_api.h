#ifndef FEATURE_API_H
#define FEATURE_API_H

// EZ per-file feature API - hides all vim isolation complexity.
// Feature author just includes this header, defines a trigger, and assigns a hotkey.
// All vim mode/seq/lb isolation is handled in core/vim_view.c - no per-file boilerplate.

#include "core/vim_view.h"
#include "core/types.h"
#include "core/modal.h"

// One-liner to show a filtered list prompt with fully isolated vim state.
// Replaces direct Platform_PromptUser calls for new features.
static inline char* Feature_ShowPrompt(
    const char* title,
    const char** items, int count,
    int height, int width, int item_height,
    RefreshCallback refresh, EnterCallback enter, CtrlEnterCallback ctrl_enter,
    TabCallback tab, CopyCallback copy, RenderCallback render,
    EditCallback edit, FilterCallback filter, HelpCallback help)
{
    VimViewSpec spec = {
        .title = title,
        .items = items,
        .count = count,
        .height = height,
        .width = width,
        .item_height = item_height,
        .refresh = refresh,
        .enter = enter,
        .ctrl_enter = ctrl_enter,
        .tab = tab,
        .copy = copy,
        .render = render,
        .edit = edit,
        .filter = filter,
        .help = help
    };
    return VimView_ShowPrompt(&spec);
}

// Simplified version for common case (just filter + enter)
static inline char* Feature_ShowSimpleList(
    const char* title,
    const char** items, int count,
    RefreshCallback refresh, EnterCallback enter)
{
    return Feature_ShowPrompt(title, items, count, 920, 650, 32,
        refresh, enter, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

// Hotkey helper - use in feature .c file at top-level:
// FEATURE_HOTKEY('K', HOTKEY_MOD_ALT, TriggerMyFeature);
// No need to edit hotkey_definitions.c
// The macro auto-registers before main() via constructor.

#ifndef FEATURE_HOTKEY
#define FEATURE_HOTKEY(vk_, mod_, handler_) \
    __attribute__((constructor)) static void _feature_hotkey_auto_##handler_(void) { \
        Feature_RegisterHotkey((vk_), (mod_), (handler_), #handler_, NULL); \
    }
#endif

#ifndef FEATURE_HOTKEY_IF
#define FEATURE_HOTKEY_IF(vk_, mod_, handler_, enabled_fn_) \
    __attribute__((constructor)) static void _feature_hotkey_auto_##handler_(void) { \
        Feature_RegisterHotkey((vk_), (mod_), (handler_), #handler_, (enabled_fn_)); \
    }
#endif

// For custom windows that are not list prompts but need isolated vim text editing:
// Create a VimView, set active, run your X11 loop calling Linux_HandleVimKey via VimContext.
// Example:
//   VimView* v = VimView_CreateCustom("MyWindow", 900, 600);
//   VimView_SetActive(v);
//   // ... XCreateSimpleWindow, XNextEvent, CheckVimMotion via VimView_GetActive() ...
//   VimView_Destroy(v);
static inline VimView* Feature_CreateCustomView(const char* title, int w, int h, bool has_listbox) {
    return VimView_Create(has_listbox, title, w, h);
}

#endif
