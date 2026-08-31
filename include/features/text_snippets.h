#ifndef TEXT_SNIPPETS_H
#define TEXT_SNIPPETS_H

#include "core/flapi.h"
#include "core/platform.h"
#include "features/window_switcher.h"

static const TextSnippet G_TEXT_SNIPPETS[] = {
    { 'O', HOTKEY_MOD_ALT_SHIFT, "*", "seifisbad@gmail.com" },
    { 'P', HOTKEY_MOD_ALT_SHIFT, "*", "i_am_retarded1" },
};
static const unsigned int TEXT_SNIPPET_COUNT = sizeof(G_TEXT_SNIPPETS) / sizeof(G_TEXT_SNIPPETS[0]);

bool CheckTextSnippetBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    int idx = fl_find_text_snippet_idx(pressed_vk, modifiers, activeExe, G_TEXT_SNIPPETS, TEXT_SNIPPET_COUNT);
    if (idx < 0) return false;
    fl_copy_into_clipboard(G_TEXT_SNIPPETS[idx].text);
    if (MessageAfterEachHotkey) {
        char buffer[128];
        snprintf(buffer,sizeof(buffer),"[TextSnippet] %s\n", G_TEXT_SNIPPETS[idx].text);
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    }
    return true;
}

static inline void text_snippets_init(void) {
}

#endif
