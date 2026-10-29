#include "core/cap_registry.h"
#include "deps/log.h"
#include "core/feature_loader.h"
#include "helpers/helpers_owner.h"
#include "core/timer.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    char key[CAP_KEY_LEN];
    char owner[512];
    unsigned abi;
    void *fn;
} Cap;

static Cap g_caps[CAP_MAX];
static int g_cap_count = 0;
static unsigned g_cap_generation = 1;

unsigned cap_generation(void) { 
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "cap_generation"); return g_cap_generation; }
}

static void cap_add_provides(const char *owner, const char *key) {
    FeatureLoader_ManifestAddProvides(owner, key);
}

void cap_register_abi(const char *key, unsigned abi, void *fn) {
    if (!key || !key[0] || !fn) return;

    char owner[OWNER_PATH_MAX];
    Owner_ResolveForFunc(fn, owner, sizeof(owner));

    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].fn == fn) return;
        log_info("cap '%s' replaced (was abi=%u now abi=%u)", key, g_caps[i].abi, abi);
        snprintf(g_caps[i].key, CAP_KEY_LEN, "%s", key);
        snprintf(g_caps[i].owner, sizeof(g_caps[i].owner), "%s", owner);
        g_caps[i].abi = abi;
        g_caps[i].fn = fn;
        g_cap_generation++;
        if (owner[0]) cap_add_provides(owner, key);
        return;
    }

    if (g_cap_count >= CAP_MAX) {
        PROFILE_START(_pt);
        log_error("cap table full, dropping '%s'", key);
        { PROFILE_END_VAR(_pt, "f"); return; }
    }
    Cap *c = &g_caps[g_cap_count++];
    snprintf(c->key, CAP_KEY_LEN, "%s", key);
    snprintf(c->owner, sizeof(c->owner), "%s", owner);
    c->abi = abi;
    c->fn = fn;
    g_cap_generation++;
    log_info("cap '%s' registered abi=%u owner='%s'", key, abi, owner[0] ? owner : "?");
    if (owner[0]) cap_add_provides(owner, key);
}

void cap_register(const char *key, void *fn) {
    cap_register_abi(key, CAP_ABI_V1, fn);
}

void *cap_resolve_abi(const char *key, unsigned abi) {
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].abi != abi) {
            PROFILE_START(_pt);
            log_warn("cap '%s' abi mismatch want=%u have=%u", key, abi, g_caps[i].abi);
            { PROFILE_END_VAR(_pt, "f"); return NULL; }
        }
        return g_caps[i].fn;
    }
    return NULL;
}

void *cap_resolve(const char *key) {
    return cap_resolve_abi(key, CAP_ABI_V1);
}

void cap_bind_declare(const char *key, void *consumer) {
    PROFILE_START(_pt);
    if (!key || !key[0] || !consumer) { PROFILE_END_VAR(_pt, "cap_bind_declare"); return; }

    char consumer_path[OWNER_PATH_MAX];
    Owner_ResolveForFunc(consumer, consumer_path, sizeof(consumer_path));
    if (!consumer_path[0]) { PROFILE_END_VAR(_pt, "cap_bind_declare"); return; }

    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].abi != CAP_ABI_V1) { PROFILE_END_VAR(_pt, "cap_bind_declare"); return; }
        if (!g_caps[i].owner[0]) { PROFILE_END_VAR(_pt, "cap_bind_declare"); return; }
        char provider[64];
        FeatureLoader_OwnerFeatureName(g_caps[i].owner, provider, sizeof(provider));
        if (!provider[0]) { PROFILE_END_VAR(_pt, "cap_bind_declare"); return; }
        FeatureLoader_ManifestAddDepends(consumer_path, provider);
        PROFILE_END_VAR(_pt, "cap_bind_declare");
        return;
    }
    PROFILE_END_VAR(_pt, "cap_bind_declare");
}

void cap_purge_owner(const char *path_a, const char *path_b) {
    PROFILE_START(_pt);
    if ((!path_a || !path_a[0]) && (!path_b || !path_b[0])) { PROFILE_END_VAR(_pt, "cap_purge_owner"); return; }

    int n = 0;
    for (int i = 0; i < g_cap_count; i++) {
        if (Owner_ModuleMatches(g_caps[i].owner, path_a, path_b)) {
            continue;
        }
        g_caps[n++] = g_caps[i];
    }
    int dropped = g_cap_count - n;
    g_cap_count = n;
    if (dropped > 0) {
        g_cap_generation++;
    }
    PROFILE_END_VAR(_pt, "cap_purge_owner");
}

void cap_dump(void) {
    PROFILE_START(_pt);
    for (int i = 0; i < g_cap_count; i++) {
        char ob[OWNER_PATH_MAX];
        Owner_ModuleBasename(g_caps[i].owner, ob, sizeof(ob));
    }
    PROFILE_END_VAR(_pt, "cap_dump");
}
