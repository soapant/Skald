#include "core/feature_loader.h"
#include "core/platform.h"
#include "platform/lib.h"
#include "config.h"
#include "deps/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONFIG_SRC "config/config.c"
#define CONFIG_START_MARK "// <flint-config-generated>"
#define CONFIG_END_MARK "// </flint-config-generated>"

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
    ApplyFn apply = (ApplyFn)Platform_GetSymbol(h, "ConfigApply");
    if (!apply) {
        log_error("config %s has no ConfigApply, keeping current", uniq);
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

static void cfg_put(char* buf, size_t cap, size_t* off, const char* fmt, ...) {
    if (*off >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n > 0) *off += (size_t)n;
}

static void cfg_put_escaped(char* buf, size_t cap, size_t* off, const char* s) {
    cfg_put(buf, cap, off, "\"");
    for (const char* p = s ? s : ""; *p; p++) {
        if (*p == '\\' || *p == '"') cfg_put(buf, cap, off, "\\%c", *p);
        else if (*p == '\n') cfg_put(buf, cap, off, "\\n");
        else if (*p == '\r') cfg_put(buf, cap, off, "\\r");
        else cfg_put(buf, cap, off, "%c", *p);
    }
    cfg_put(buf, cap, off, "\"");
}

static void cfg_put_float(char* buf, size_t cap, size_t* off, const char* name, float v) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%.6g", (double)v);
    if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E') && !strchr(tmp, 'n') && !strchr(tmp, 'N')) {
        cfg_put(buf, cap, off, "    %s = %s.0f;\n", name, tmp);
    } else {
        cfg_put(buf, cap, off, "    %s = %sf;\n", name, tmp);
    }
}

static size_t config_emit_region(char* buf, size_t cap) {
    size_t off = 0;
    cfg_put(buf, cap, &off, "static const PathVar user_path_vars[] = {\n");
    for (unsigned int i = 0; i < PATH_VAR_COUNT && G_PATH_VARS; i++) {
        cfg_put(buf, cap, &off, "    {");
        cfg_put_escaped(buf, cap, &off, G_PATH_VARS[i].name);
        cfg_put(buf, cap, &off, ", ");
        cfg_put_escaped(buf, cap, &off, G_PATH_VARS[i].win32_value);
        cfg_put(buf, cap, &off, ", ");
        cfg_put_escaped(buf, cap, &off, G_PATH_VARS[i].linux_value);
        cfg_put(buf, cap, &off, "},\n");
    }
    cfg_put(buf, cap, &off, "};\n\n");
    cfg_put(buf, cap, &off, "static const AliasEntry user_aliases[] = {\n");
    for (size_t i = 0; i < ALIAS_ENTRY_COUNT && Aliases; i++) {
        cfg_put(buf, cap, &off, "    {");
        cfg_put_escaped(buf, cap, &off, Aliases[i].alias);
        cfg_put(buf, cap, &off, ", ");
        cfg_put_escaped(buf, cap, &off, Aliases[i].text);
        cfg_put(buf, cap, &off, ", ");
        cfg_put_escaped(buf, cap, &off, Aliases[i].feature);
        cfg_put(buf, cap, &off, "},\n");
    }
    cfg_put(buf, cap, &off, "};\n\n");
    cfg_put(buf, cap, &off, "void ConfigApply(void) {\n");
    cfg_put(buf, cap, &off, "    Tray = %s;\n", Tray ? "true" : "false");
    cfg_put(buf, cap, &off, "    EnableVimMode = %s;\n", EnableVimMode ? "true" : "false");
    cfg_put(buf, cap, &off, "    RememberLastIndex = %s;\n", RememberLastIndex ? "true" : "false");
    cfg_put(buf, cap, &off, "    KeepSelectionAfterClose = %s;\n", KeepSelectionAfterClose ? "true" : "false");
    cfg_put(buf, cap, &off, "    MessageAfterEachHotkey = %s;\n", MessageAfterEachHotkey ? "true" : "false");
    cfg_put(buf, cap, &off, "    AllowOtherFeatureTogglesInFeature = %s;\n", AllowOtherFeatureTogglesInFeature ? "true" : "false");
    cfg_put(buf, cap, &off, "    ModePersistsThroughFeatures = %s;\n", ModePersistsThroughFeatures ? "true" : "false");
    cfg_put(buf, cap, &off, "    TimeOutForMessages = %d;\n", TimeOutForMessages);
    cfg_put(buf, cap, &off, "    DoTimeDebugs = %s;\n", DoTimeDebugs ? "true" : "false");
    cfg_put(buf, cap, &off, "    DebugMode = %s;\n", DebugMode ? "true" : "false");
    cfg_put(buf, cap, &off, "    RoundedCorners = %s;\n", RoundedCorners ? "true" : "false");
    cfg_put(buf, cap, &off, "    CornerRadius = %d;\n", CornerRadius);
    cfg_put(buf, cap, &off, "    EnableShaders = %s;\n", EnableShaders ? "true" : "false");
    cfg_put(buf, cap, &off, "    ShaderPreset = %d;\n", ShaderPreset);
    cfg_put_float(buf, cap, &off, "ShaderIntensity", ShaderIntensity);
    cfg_put(buf, cap, &off, "    TimeBetweenHotReload = %d;\n", TimeBetweenHotReload);
    cfg_put_float(buf, cap, &off, "WindowOpacity", WindowOpacity);
    cfg_put(buf, cap, &off, "    EditPopUpPos = ");
    cfg_put_escaped(buf, cap, &off, EditPopUpPos);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    EditPopUpWidthMin = %d;\n", EditPopUpWidthMin);
    cfg_put(buf, cap, &off, "    PageUpDownLen = %d;\n", PageUpDownLen);
    cfg_put(buf, cap, &off, "    UsedFont = ");
    cfg_put_escaped(buf, cap, &off, UsedFont);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    FontSize = %d;\n", FontSize);
    cfg_put(buf, cap, &off, "    IconSize = %d;\n", IconSize);
    cfg_put(buf, cap, &off, "    MaxFeatures = %d;\n", MaxFeatures);
    cfg_put(buf, cap, &off, "    VimFontSize = %d;\n", VimFontSize);
    cfg_put(buf, cap, &off, "    PaddingForScrolling = %d;\n", PaddingForScrolling);
    cfg_put(buf, cap, &off, "    UsedTerminal = ");
    cfg_put_escaped(buf, cap, &off, UsedTerminal);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    UsedShell = ");
    cfg_put_escaped(buf, cap, &off, UsedShell);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    UsedBrowser = ");
    cfg_put_escaped(buf, cap, &off, UsedBrowser);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    UsedImageViewer = ");
    cfg_put_escaped(buf, cap, &off, UsedImageViewer);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    UsedVideoPlayer = ");
    cfg_put_escaped(buf, cap, &off, UsedVideoPlayer);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    UsedSearchEngine = ");
    cfg_put_escaped(buf, cap, &off, UsedSearchEngine);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    FeaturePath = ");
    cfg_put_escaped(buf, cap, &off, FeaturePath);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    IconPath = ");
    cfg_put_escaped(buf, cap, &off, IconPath);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    ControlHotkey = ");
    cfg_put_escaped(buf, cap, &off, ControlHotkey);
    cfg_put(buf, cap, &off, ";\n");
    cfg_put(buf, cap, &off, "    FullTextNeedsKeyPress = %s;\n", FullTextNeedsKeyPress ? "true" : "false");
    cfg_put(buf, cap, &off, "    MessagesEnabled = %s;\n", MessagesEnabled ? "true" : "false");
    cfg_put(buf, cap, &off, "    RainbowText = %s;\n", RainbowText ? "true" : "false");
    cfg_put(buf, cap, &off, "    AlternatingText = %s;\n", AlternatingText ? "true" : "false");
    cfg_put(buf, cap, &off, "    VimModeText = %s;\n", VimModeText ? "true" : "false");
    cfg_put(buf, cap, &off, "    WindowTitleText = %s;\n", WindowTitleText ? "true" : "false");
    cfg_put(buf, cap, &off, "    ColoredVimModeText = %s;\n", ColoredVimModeText ? "true" : "false");
    cfg_put(buf, cap, &off, "    GradientItems = %d;\n", GradientItems);
    cfg_put_float(buf, cap, &off, "GradientBrightness", GradientBrightness);
    cfg_put(buf, cap, &off, "    CloseOnLossOfFocus = %s;\n", CloseOnLossOfFocus ? "true" : "false");
    ColorRef cols[] = {COLOR_BG, COLOR_TEXT, COLOR_ALTERNATE, COLOR_SELECTED_TEXT, COLOR_ALIAS_TEXT, COLOR_ERROR, COLOR_NORMAL, COLOR_TIP, COLOR_INSERT_MODE, COLOR_NORMAL_MODE, COLOR_VISUAL_MODE};
    const char* names[] = {"COLOR_BG", "COLOR_TEXT", "COLOR_ALTERNATE", "COLOR_SELECTED_TEXT", "COLOR_ALIAS_TEXT", "COLOR_ERROR", "COLOR_NORMAL", "COLOR_TIP", "COLOR_INSERT_MODE", "COLOR_NORMAL_MODE", "COLOR_VISUAL_MODE"};
    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
        cfg_put(buf, cap, &off, "    %s = RGB(%u, %u, %u);\n", names[i], cols[i] & 0xFFu, (cols[i] >> 8) & 0xFFu, (cols[i] >> 16) & 0xFFu);
    }
    cfg_put(buf, cap, &off, "    G_PATH_VARS = user_path_vars;\n");
    cfg_put(buf, cap, &off, "    PATH_VAR_COUNT = %u;\n", PATH_VAR_COUNT);
    cfg_put(buf, cap, &off, "    Aliases = user_aliases;\n");
    cfg_put(buf, cap, &off, "    ALIAS_ENTRY_COUNT = %u;\n", (unsigned)ALIAS_ENTRY_COUNT);
    cfg_put(buf, cap, &off, "}\n");
    return off;
}

static long long g_cfg_saved_mtime = 0;
static long long g_cfg_saved_size = 0;
static int g_cfg_saved_valid = 0;

void Config_Save(void) {
    static char region[65536];
    size_t n = config_emit_region(region, sizeof(region));
    if (n == 0 || n >= sizeof(region)) { log_error("config emit failed"); return; }
    FILE* f = fopen(CONFIG_SRC, "rb");
    if (!f) { log_error("config open %s failed", CONFIG_SRC); return; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0 || fsize > 1048576) { fclose(f); log_error("config size bad"); return; }
    char* orig = malloc((size_t)fsize + 1);
    if (!orig) { fclose(f); return; }
    size_t got = fread(orig, 1, (size_t)fsize, f);
    fclose(f);
    orig[got] = '\0';
    char* start = strstr(orig, CONFIG_START_MARK);
    char* end = start ? strstr(start, CONFIG_END_MARK) : NULL;
    if (!start || !end || end < start) { free(orig); log_error("config markers missing, not touching file"); return; }
    char* start_eol = strchr(start, '\n');
    if (!start_eol) { free(orig); return; }
    start_eol++;
    size_t head = (size_t)(start_eol - orig);
    size_t tail = got - (size_t)(end - orig);
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", CONFIG_SRC, (int)getpid());
    FILE* out = fopen(tmp, "wb");
    if (!out) { free(orig); return; }
    bool ok = fwrite(orig, 1, head, out) == head;
    if (ok) ok = fwrite(region, 1, n, out) == n;
    if (ok) ok = fwrite(end, 1, tail, out) == tail;
    if (fclose(out) != 0) ok = false;
    free(orig);
    if (!ok) { remove(tmp); log_error("config write failed"); return; }
    if (rename(tmp, CONFIG_SRC) != 0) { remove(tmp); log_error("config rename failed"); return; }
    struct stat st;
    if (stat(CONFIG_SRC, &st) == 0) {
        g_cfg_saved_mtime = (long long)st.st_mtime;
        g_cfg_saved_size = (long long)st.st_size;
        g_cfg_saved_valid = 1;
    }
    log_info("config saved");
}

static long long g_cfg_base_mtime = 0;
static long long g_cfg_base_size = 0;
static int g_cfg_base_init = 0;
static long long g_cfg_change_at = 0;

void Config_PollWatcher(void) {
    struct stat st;
    if (stat(CONFIG_SRC, &st) != 0) return;
    long long mt = (long long)st.st_mtime;
    long long sz = (long long)st.st_size;
    if (!g_cfg_base_init) { g_cfg_base_mtime = mt; g_cfg_base_size = sz; g_cfg_base_init = 1; return; }
    if (mt != g_cfg_base_mtime || sz != g_cfg_base_size) {
        if (g_cfg_saved_valid && mt == g_cfg_saved_mtime && sz == g_cfg_saved_size) {
            g_cfg_saved_valid = 0;
            g_cfg_base_mtime = mt;
            g_cfg_base_size = sz;
            g_cfg_change_at = 0;
            return;
        }
        g_cfg_base_mtime = mt;
        g_cfg_base_size = sz;
        g_cfg_change_at = Platform_GetWallClockMs();
        return;
    }
    if (g_cfg_change_at == 0) return;
    if (Platform_GetWallClockMs() - g_cfg_change_at < 1000) return;
    g_cfg_change_at = 0;
    Config_Reload();
}
