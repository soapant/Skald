#ifndef TEXT_SNIPPETS_H
#define TEXT_SNIPPETS_H

#include "core/flapi.h"

void copy_seifisbad(void) {
    fl_copy_into_clipboard("seifisbad@gmail.com");
    if (MessageAfterEachHotkey) {
        char buffer[128];
        snprintf(buffer,sizeof(buffer),"[TextSnippet] %s\n", "seifisbad@gmail.com");
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    }
}
void copy_password(void) {
    fl_copy_into_clipboard("i_am_retarded1");
    if (MessageAfterEachHotkey) {
        char buffer[128];
        snprintf(buffer,sizeof(buffer),"[TextSnippet] %s\n", "i_am_retarded1");
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    }
}

static inline void text_snippets_init(void) {
    fl_register_hotkey("Alt+Shift+O", copy_seifisbad, NULL, NULL);
    fl_register_hotkey("Alt+Shift+P", copy_password, NULL, NULL);
}

#endif
