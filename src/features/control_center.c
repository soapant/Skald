#include "core/flapi.h"
#include <string.h>

static char g_sel[256]={0};

static bool control_enter(const char* selection, ListBoxInfo* lb) {
    if (selection) strncpy(g_sel, selection, sizeof(g_sel)-1);
    else g_sel[0]=0;
    return true;
}

static bool control_render(PromptState* state) {
    ListBoxInfo* lb = &state->listbox;
    int line_h = fl_get_text_height(NULL);
    FlListSpec spec = {
        .lb = lb,
        .item_height = line_h * 3
    };
    fl_place_editbox();
    if (!fl_listbox_begin(&spec)) return false;
    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {

    }
    fl_listbox_end();
}


static void TriggerControl(void) {
    const char* names[128]; void (*cbs[128])(void);
    int n= fl_get_feature_list(names,cbs,128);
    g_sel[0]=0;
    PROMPT(.title="⚡ Control",
           .enter=control_enter);
    for (int i=0;i<n;i++) if (strcmp(g_sel,names[i])==0 && cbs[i]) { cbs[i](); return; }
}

FLINT_FEATURE_INIT {
    fl_register_hotkey((char*)ControlHotkey, TriggerControl);
}
