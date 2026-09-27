#include "core/flapi.h"
#include "config.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include <string.h>

static char g_sel[256]={0};

static void control_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* names[MaxFeatures]; int n=fl_get_feature_list(names,NULL,128);
    PopulateListBox(lb, filter, names, n, NULL);
}
static bool control_enter(const char* selection, ListBoxInfo* lb) {
    (void)lb;
    if (selection) strncpy(g_sel, selection, sizeof(g_sel)-1);
    else g_sel[0]=0;
    return true;
}
static void TriggerControl(void) {
    const char* names[128]; void (*cbs[128])(void);
    int n= fl_get_feature_list(names,cbs,128);
    g_sel[0]=0;
    PROMPT(.title="⚡ Control",
           .refresh=control_refresh,
           .enter=control_enter);
    for (int i=0;i<n;i++) if (strcmp(g_sel,names[i])==0 && cbs[i]) { cbs[i](); return; }
}

FLINT_FEATURE_INIT {
    // temporary
    // fl_register_hotkey((char*)ControlHotkey, TriggerControl);
    // fl_register_feature("Control Center", TriggerControl);
}
