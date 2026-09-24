#include "core/feature_loader.h"
#include "core/platform.h"
#include "platform/lib.h"
#include "deps/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CONFIG_SRC "config/config.c"

static const char* config_out_path(void) {
    static char path[256];
    snprintf(path, sizeof(path), "build/config%s", Platform_LibExtension());
    return path;
}

static bool config_copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (!ok) remove(dst);
    return ok;
}

typedef struct {
    LibHandle handle;
    char uniq[256];
} ConfigLib;

static ConfigLib g_cfg_libs[2];
static int g_cfg_count = 0;
static int g_cfg_seq = 0;

static bool config_load_copy(const char* uniq, LibHandle* out) {
    LibHandle h = Platform_LoadLibrary(uniq);
    if (!h) {
        log_error("config dlopen %s failed", uniq);
        return false;
    }
    typedef void (*ApplyFn)(void);
    ApplyFn apply = (ApplyFn)Platform_GetSymbol(h, "FlintConfigApply");
    if (!apply) {
        log_error("config %s has no FlintConfigApply, keeping current", uniq);
        Platform_UnloadLibrary(h);
        return false;
    }
    apply();
    *out = h;
    return true;
}

static bool config_build_load_store(void) {
    const char* out = config_out_path();
    if (!Platform_BuildFeature(CONFIG_SRC, out)) {
        log_error("config compile failed, keeping current");
        return false;
    }
    char uniq[256];
    snprintf(uniq, sizeof(uniq), "build/config.hot%d%s", ++g_cfg_seq, Platform_LibExtension());
    if (!config_copy_file(out, uniq)) {
        log_error("config copy failed, keeping current");
        return false;
    }
    LibHandle h = 0;
    if (!config_load_copy(uniq, &h)) {
        remove(uniq);
        return false;
    }
    if (g_cfg_count >= 2) {
        Platform_UnloadLibrary(g_cfg_libs[0].handle);
        remove(g_cfg_libs[0].uniq);
        g_cfg_libs[0] = g_cfg_libs[1];
        g_cfg_count = 1;
    }
    g_cfg_libs[g_cfg_count].handle = h;
    strncpy(g_cfg_libs[g_cfg_count].uniq, uniq, sizeof(g_cfg_libs[g_cfg_count].uniq) - 1);
    g_cfg_libs[g_cfg_count].uniq[sizeof(g_cfg_libs[g_cfg_count].uniq) - 1] = '\0';
    g_cfg_count++;
    return true;
}

bool Config_Load(void) {
    if (!config_build_load_store()) {
        log_error("config load failed, running on built-in defaults");
        return false;
    }
    return true;
}

bool Config_Reload(void) {
    if (!config_build_load_store()) return false;
    FeatureLoader_ReloadAll();
    return true;
}

void Config_Unload(void) {
    for (int i = 0; i < g_cfg_count; i++) {
        if (g_cfg_libs[i].handle) Platform_UnloadLibrary(g_cfg_libs[i].handle);
        if (g_cfg_libs[i].uniq[0]) remove(g_cfg_libs[i].uniq);
        g_cfg_libs[i].handle = 0;
        g_cfg_libs[i].uniq[0] = '\0';
    }
    g_cfg_count = 0;
}
