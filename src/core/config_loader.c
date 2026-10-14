#include "platform/platform.h"
#include "platform/lib.h"
#include "config.h"
#include "core/theme.h"
#include "deps/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "core/window_manager.h"
#include "core/timer.h"

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

static char* heap_dup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* d = (char*)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
static AliasEntry* g_heap_aliases = NULL;
static size_t g_heap_alias_count = 0;
static char *g_heap_used_font = NULL, *g_heap_used_terminal = NULL, *g_heap_used_shell = NULL;
static char *g_heap_used_browser = NULL;
static char *g_heap_used_search = NULL, *g_heap_feature_path = NULL, *g_heap_icon_path = NULL;
static char *g_heap_control_hotkey = NULL, *g_heap_edit_popup_pos = NULL, *g_heap_theme = NULL;
static void heapify_config(void) {
    char *n_font = heap_dup(UsedFont), *n_term = heap_dup(UsedTerminal), *n_shell = heap_dup(UsedShell);
    char *n_search = heap_dup(UsedSearchEngine), *n_feat = heap_dup(FeaturePath),*n_browser = heap_dup(UsedBrowser),*n_icon = heap_dup(IconPath);
    char *n_hotk = heap_dup(ControlHotkey), *n_pos = heap_dup(EditPopUpPos), *n_theme = heap_dup(Theme);
    AliasEntry *n_alias = NULL; size_t n_alias_cnt = 0;
    if (Aliases && ALIAS_ENTRY_COUNT) {
        n_alias = (AliasEntry*)calloc(ALIAS_ENTRY_COUNT, sizeof(AliasEntry));
        if (n_alias) { n_alias_cnt = ALIAS_ENTRY_COUNT; for (size_t i=0;i<n_alias_cnt;i++){ n_alias[i].alias=heap_dup(Aliases[i].alias); n_alias[i].text=heap_dup(Aliases[i].text); n_alias[i].feature=heap_dup(Aliases[i].feature); } }
    }
    char *old_font=g_heap_used_font, *old_term=g_heap_used_terminal, *old_shell=g_heap_used_shell, *old_browser=g_heap_used_browser, *old_search=g_heap_used_search, *old_feat=g_heap_feature_path, *old_icon=g_heap_icon_path, *old_hotk=g_heap_control_hotkey, *old_pos=g_heap_edit_popup_pos, *old_theme=g_heap_theme;
    AliasEntry *old_alias=g_heap_aliases; size_t old_alias_cnt=g_heap_alias_count;
    if (n_font) { UsedFont=n_font; g_heap_used_font=n_font; } else g_heap_used_font=old_font, old_font=NULL;
    if (n_term) { UsedTerminal=n_term; g_heap_used_terminal=n_term; } else g_heap_used_terminal=old_term, old_term=NULL;
    if (n_shell) { UsedShell=n_shell; g_heap_used_shell=n_shell; } else g_heap_used_shell=old_shell, old_shell=NULL;
    if (n_browser) { UsedBrowser=n_browser; g_heap_used_browser=n_browser; } else g_heap_used_browser=old_browser, old_browser=NULL;
    if (n_search) { UsedSearchEngine=n_search; g_heap_used_search=n_search; } else g_heap_used_search=old_search, old_search=NULL;
    if (n_feat) { FeaturePath=n_feat; g_heap_feature_path=n_feat; } else g_heap_feature_path=old_feat, old_feat=NULL;
    if (n_icon) { IconPath=n_icon; g_heap_icon_path=n_icon; } else g_heap_icon_path=old_icon, old_icon=NULL;
    if (n_hotk) { ControlHotkey=n_hotk; g_heap_control_hotkey=n_hotk; } else g_heap_control_hotkey=old_hotk, old_hotk=NULL;
    if (n_pos) { EditPopUpPos=n_pos; g_heap_edit_popup_pos=n_pos; } else g_heap_edit_popup_pos=old_pos, old_pos=NULL;
    if (n_theme) { Theme=n_theme; g_heap_theme=n_theme; } else g_heap_theme=old_theme, old_theme=NULL;
    if (n_alias) { Aliases=n_alias; g_heap_aliases=n_alias; g_heap_alias_count=n_alias_cnt; } else { g_heap_aliases=old_alias; g_heap_alias_count=old_alias_cnt; old_alias=NULL; }
    (void)old_font; (void)old_term; (void)old_shell; (void)old_browser; (void)old_search; (void)old_feat; (void)old_icon; (void)old_hotk; (void)old_pos; (void)old_theme;
    (void)old_alias;
}
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
    {
        ColorRef cfg_cursor = COLOR_CURSOR;
        ColorRef cfg_visual_cursor = COLOR_VISUAL_CURSOR;
        Theme_Apply();
        if (Theme_ColorPinned("COLOR_CURSOR")) COLOR_CURSOR = cfg_cursor;
        if (Theme_ColorPinned("COLOR_VISUAL_CURSOR")) COLOR_VISUAL_CURSOR = cfg_visual_cursor;
    }
    heapify_config();
    *out = h;
    return true;
}

static bool config_output_is_fresh(const char* out) {
    struct stat src_st, out_st;
    if (stat(CONFIG_SRC, &src_st) != 0) return false;
    if (stat(out, &out_st) != 0) return false;
    return out_st.st_mtime >= src_st.st_mtime;
}

static bool config_build_load_store(void) {
    PROFILE_START(timer);
    const char* out = config_out_path();
    if (config_output_is_fresh(out)) {
        log_info("config .so is newer than source, skipping compile");
    } else {
        PROFILE_START(t_cfgcompile);
        bool ok = Platform_BuildFeature(CONFIG_SRC, out);
        PROFILE_END_VAR(t_cfgcompile, "config:compile_config_c");
        if (!ok) {
            log_error("config compile failed, keeping current");
            PROFILE_END_VAR(timer, "config:build_load_store");
            return false;
        }
    }
    char uniq[256];
    snprintf(uniq, sizeof(uniq), "build/config.hot%d%s", ++g_cfg_seq, Platform_LibExtension());
    {
        PROFILE_START(t_cfgcopy);
        bool ok = config_copy_file(out, uniq);
        PROFILE_END_VAR(t_cfgcopy, "config:copy_to_hot");
        if (!ok) {
            log_error("config copy failed, keeping current");
            PROFILE_END_VAR(timer, "config:build_load_store");
            return false;
        }
    }
    LibHandle h = 0;
    {
        PROFILE_START(t_cfgdl);
        bool ok = config_load_copy(uniq, &h);
        PROFILE_END_VAR(t_cfgdl, "config:dlopen_apply");
        if (!ok) {
            remove(uniq);
            PROFILE_END_VAR(timer, "config:build_load_store");
            return false;
        }
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
    PROFILE_END_VAR(timer, "config:build_load_store TOTAL");
    return true;
}

bool Config_Load(void) {
    PROFILE_START(timer);
    bool ok = config_build_load_store();
    PROFILE_END_VAR(timer, "config:Config_Load");
    if (!ok) {
        log_error("config load failed, running on built-in defaults");
        return false;
    }
    return true;
}

static long long g_cfg_last_reload_ms = 0;
#ifdef _WIN32
static volatile LONG g_cfg_reload_lock = 0;
#endif
bool Config_Reload(void) {
    WindowManager* _wm2 = WindowManager_Get();
    if (_wm2 && _wm2->popup_count > 0) { log_warn("cfg reload blocked: %d popup(s) open", _wm2->popup_count); return false; }
#ifdef _WIN32
    if (InterlockedCompareExchange(&g_cfg_reload_lock, 1, 0) != 0) return false;
#endif
    long long now = Platform_GetWallClockMs();
    long long debounce = (long long)TimeBetweenHotReload * 1000;
    if (debounce < 250) debounce = 250;
    if (g_cfg_last_reload_ms && now - g_cfg_last_reload_ms < debounce) {
        log_warn("cfg reload blocked: debounce, %lldms since last", (long long)(now - g_cfg_last_reload_ms));
#ifdef _WIN32
        InterlockedExchange(&g_cfg_reload_lock, 0);
#endif
        return false;
    }
    if (!config_build_load_store()) {
        log_warn("cfg reload blocked: build/load failed, keeping old values");
#ifdef _WIN32
        InterlockedExchange(&g_cfg_reload_lock, 0);
#endif
        return false;
    }
    log_warn("cfg reload applied: intensity=%.3f brightness=%.3f preset=%d shaders=%d", (double)ShaderIntensity, (double)ShaderBrightness, ShaderPreset, EnableShaders ? 1 : 0);
    g_cfg_last_reload_ms = Platform_GetWallClockMs();
    Platform_ResetUIScale();
#ifdef _WIN32
    InterlockedExchange(&g_cfg_reload_lock, 0);
#endif
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

static void cfg_put_str_var(char* buf, size_t cap, size_t* off, const char* name, const char* value) {
    if (Config_IsAutoResolvedApp(name)) value = "";
    cfg_put(buf, cap, off, "    %s = ", name);
    cfg_put_escaped(buf, cap, off, value);
    cfg_put(buf, cap, off, ";\n");
}

static size_t config_emit_region(char* buf, size_t cap) {
    size_t off = 0;
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
    cfg_put_float(buf, cap, &off, "ShaderBrightness", ShaderBrightness);
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
    cfg_put(buf, cap, &off, "    FontSize = %d;\n", Platform_GetUnscaledFontSize());
    cfg_put(buf, cap, &off, "    IconSize = %d;\n", IconSize);
    cfg_put(buf, cap, &off, "    WidgetGap = %d;\n", WidgetGap);
    cfg_put(buf, cap, &off, "    MaxFeatures = %d;\n", MaxFeatures);
    cfg_put(buf, cap, &off, "    VimFontSize = %d;\n", VimFontSize);
    cfg_put(buf, cap, &off, "    PaddingForScrolling = %d;\n", PaddingForScrolling);
    cfg_put_str_var(buf, cap, &off, "UsedTerminal", UsedTerminal);
    cfg_put_str_var(buf, cap, &off, "UsedShell", UsedShell);
    cfg_put_str_var(buf, cap, &off, "UsedBrowser", UsedBrowser);
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
    cfg_put(buf, cap, &off, "    Theme = ");
    cfg_put_escaped(buf, cap, &off, Theme);
    cfg_put(buf, cap, &off, ";\n");
    ColorRef cols[] = {COLOR_BG, COLOR_TEXT, COLOR_ALTERNATE, COLOR_SELECTED_TEXT, COLOR_ALIAS_TEXT, COLOR_ERROR, COLOR_NORMAL, COLOR_TIP, COLOR_INSERT_MODE, COLOR_NORMAL_MODE, COLOR_VISUAL_MODE, COLOR_VISUAL_CURSOR, COLOR_CURSOR};
    const char* names[] = {"COLOR_BG", "COLOR_TEXT", "COLOR_ALTERNATE", "COLOR_SELECTED_TEXT", "COLOR_ALIAS_TEXT", "COLOR_ERROR", "COLOR_NORMAL", "COLOR_TIP", "COLOR_INSERT_MODE", "COLOR_NORMAL_MODE", "COLOR_VISUAL_MODE", "COLOR_VISUAL_CURSOR", "COLOR_CURSOR"};
    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
        cfg_put(buf, cap, &off, "    %s = RGB(%u, %u, %u);\n", names[i], cols[i] & 0xFFu, (cols[i] >> 8) & 0xFFu, (cols[i] >> 16) & 0xFFu);
    }
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
    size_t body_len = (size_t)(end - start_eol);
    if (body_len == n && memcmp(orig + head, region, n) == 0) {
        free(orig);
        log_info("config unchanged, not rewriting");
        return;
    }
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
#ifdef _WIN32
    remove(CONFIG_SRC);
    if (!MoveFileExA(tmp, CONFIG_SRC, MOVEFILE_REPLACE_EXISTING)) { remove(tmp); log_error("config rename failed %lu", GetLastError()); return; }
#else
    if (rename(tmp, CONFIG_SRC) != 0) { remove(tmp); log_error("config rename failed"); return; }
#endif
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
        log_warn("cfg change detected mtime=%lld size=%lld", mt, sz);
        return;
    }
    if (g_cfg_change_at == 0) return;
    long long debounce = (long long)TimeBetweenHotReload * 1000;
    if (debounce < 250) debounce = 250;
    if (Platform_GetWallClockMs() - g_cfg_change_at < debounce) return;
    if (g_cfg_last_reload_ms && Platform_GetWallClockMs() - g_cfg_last_reload_ms < debounce) return;
    WindowManager* _wm = WindowManager_Get();
    if (_wm && _wm->popup_count > 0) { log_warn("cfg reload deferred: %d popup(s) open", _wm->popup_count); return; }
    g_cfg_change_at = 0;
#ifdef _WIN32
    HWND tray = NULL;
    if (FlintContext_Get()) tray = WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd);
    if (tray && IsWindow(tray)) { PostMessageA(tray, WM_APP + 0x42, 0, 0); }
    else Config_Reload();
#else
    Config_Reload();
#endif
}
