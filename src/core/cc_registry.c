#include "core/cc_registry.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_owner.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/feature_loader.h"

typedef struct {
    CcEntrySpec pub;
    char display_name[1024];
    char internal_name[1024];
    char icon[1024];
    char type[128];
    char owner[OWNER_PATH_MAX];
} CcEntry;

static int g_cc_entry_count = 0;
static CcEntry g_cc_entries[4096];

#define FL_CC_DEP_OWNERS 256
static char g_cc_dep_owners[FL_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int g_cc_dep_owner_count = 0;

static bool cc_dep_seen(const char *owner) {
    for (int i = 0; i < g_cc_dep_owner_count; i++)
        if (strcmp(g_cc_dep_owners[i], owner) == 0) return true;
    return false;
}

static void cc_dep_mark(const char *owner) {
    if (g_cc_dep_owner_count >= FL_CC_DEP_OWNERS) return;
    snprintf(g_cc_dep_owners[g_cc_dep_owner_count++], OWNER_PATH_MAX, "%s", owner);
}

static const char *cc_dup(char *dst, size_t cap, const char *src) {
    snprintf(dst, cap, "%s", src ? src : "");
    return dst;
}

bool add_dep(const char *dep_name, const char *owner_file) {
    if (!dep_name || !dep_name[0] || !owner_file || !owner_file[0]) return false;
    char dep[64];
    snprintf(dep, sizeof(dep), "%s", dep_name);
    char *dot = strrchr(dep, '.');
    if (dot) *dot = '\0';
    if (!dep[0]) return false;
    FeatureLoader_ManifestAddDepends(owner_file, dep);
    return true;
}

bool add_to_cc(const CcEntrySpec *spec) {
    if (!spec) return false;
    int slot = -1;
    for (int i = 0; i < g_cc_entry_count; i++) {
        if (spec->display_name && g_cc_entries[i].display_name[0] &&
            strcmp(g_cc_entries[i].display_name, spec->display_name) == 0) { slot = i; break; }
    }
    if (slot < 0) {
        if (g_cc_entry_count >= 4096) return false;
        slot = g_cc_entry_count++;
    }
    CcEntry *e = &g_cc_entries[slot];
    memset(e, 0, sizeof(*e));
    e->pub.on_enter = spec->on_enter;
    e->pub.on_ctrl_enter = spec->on_ctrl_enter;
    e->pub.on_shift_enter = spec->on_shift_enter;
    e->pub.on_ctrl_shift_enter = spec->on_ctrl_shift_enter;
    e->pub.on_ctrl_alt_enter = spec->on_ctrl_alt_enter;
    e->pub.on_shift_alt_enter = spec->on_shift_alt_enter;
    e->pub.on_tab = spec->on_tab;
    e->pub.on_action = spec->on_action;
    e->pub.display_name = cc_dup(e->display_name, sizeof(e->display_name), spec->display_name);
    char icon_asset[MAX_PATH];
    if (spec->icon && (spec->icon[0] == '/' || spec->icon[0] == '\\') &&
        Platform_ResolveAssetPath(__builtin_return_address(0), spec->icon, icon_asset, sizeof(icon_asset))) {
        e->pub.icon = cc_dup(e->icon, sizeof(e->icon), icon_asset);
    } else {
        e->pub.icon = cc_dup(e->icon, sizeof(e->icon), spec->icon);
    }
    e->pub.type = cc_dup(e->type, sizeof(e->type), spec->type);
    if (spec->internal_name) {
        e->pub.internal_name = cc_dup(e->internal_name, sizeof(e->internal_name), spec->internal_name);
    } else {
        e->pub.internal_name = cc_dup(e->internal_name, sizeof(e->internal_name), spec->display_name);
    }
    void *anchor = (void*)(uintptr_t)spec->on_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_ctrl_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_shift_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_ctrl_shift_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_ctrl_alt_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_shift_alt_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_tab;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_action;
    Owner_ResolveForFunc(anchor, e->owner, sizeof(e->owner));
    if (e->owner[0] && !cc_dep_seen(e->owner)) {
        cc_dep_mark(e->owner);
        add_dep("control_center.c", e->owner);
    }
    return true;
}


int cc_entry_count(void) {
    return g_cc_entry_count;
}

const CcEntrySpec *cc_entry_at(int index) {
    if (index < 0 || index >= g_cc_entry_count) return NULL;
    return &g_cc_entries[index].pub;
}

void cc_registry_purge_owner(const char *path_a, const char *path_b) {
    if (!path_a && !path_b) return;
    int out = 0;
    for (int i = 0; i < g_cc_entry_count; i++) {
        if (Owner_ModuleMatches(g_cc_entries[i].owner, path_a, path_b)) continue;
        if (out != i) g_cc_entries[out] = g_cc_entries[i];
        out++;
    }
    g_cc_entry_count = out;
}
