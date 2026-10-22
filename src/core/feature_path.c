#include "core/feature_path.h"
#include "core/context.h"
#include "core/timer.h"
#include "platform/platform.h"
#include "platform/lib.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int fp_lower(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static int fp_strnicmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = fp_lower((unsigned char)a[i]);
        int cb = fp_lower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
    return 0;
}

static int fp_strcasecmp(const char* a, const char* b) {
    size_t i = 0;
    for (;; i++) {
        int ca = fp_lower((unsigned char)a[i]);
        int cb = fp_lower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
}

void FeaturePath_SanitizeStem(const char* src, char* out, size_t out_len) {
    PROFILE_START(_pt);
    if (!out || out_len == 0) { PROFILE_END_VAR(_pt, "FeaturePath_SanitizeStem"); return; }
    out[0] = '\0';
    if (!src) { PROFILE_END_VAR(_pt, "FeaturePath_SanitizeStem"); return; }
    size_t w = 0;
    for (const char* p = src; *p && w + 1 < out_len; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') out[w++] = (char)(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out[w++] = c;
        else if (c == ' ') out[w++] = '_';
    }
    out[w] = '\0';
    if (w == 0 || strcmp(out, ".") == 0 || strcmp(out, "..") == 0) out[0] = '\0';
    PROFILE_END_VAR(_pt, "FeaturePath_SanitizeStem");
}

bool FeaturePath_LibBasenameStem(const char* base, char* out, size_t out_len) {
    PROFILE_START(_pt);
    if (!base || !out || out_len == 0) { PROFILE_END_VAR(_pt, "FeaturePath_LibBasenameStem"); return false; }
    char work[256];
    size_t len = strlen(base);
    if (len == 0 || len >= sizeof(work)) { PROFILE_END_VAR(_pt, "FeaturePath_LibBasenameStem"); return false; }
    memcpy(work, base, len + 1);
    for (;;) {
        if (len < 4) break;
        size_t dot_at = 0;
        for (size_t i = len; i > 0; i--) {
            if (work[i - 1] == '.') { dot_at = i - 1; break; }
        }
        if (dot_at == 0) break;
        if (fp_strnicmp(work + dot_at + 1, "hot", 3) != 0) break;
        bool digits = false;
        for (size_t i = dot_at + 4; i < len; i++) {
            if (work[i] < '0' || work[i] > '9') { digits = false; break; }
            digits = true;
        }
        if (!digits) break;
        len = dot_at;
        work[len] = '\0';
    }
    if (len >= 3 && fp_strcasecmp(work + len - 3, ".so") == 0) len -= 3;
    else if (len >= 4 && fp_strcasecmp(work + len - 4, ".dll") == 0) len -= 4;
    else if (len >= 5 && fp_strcasecmp(work + len - 5, ".dylib") == 0) len -= 5;
    else { PROFILE_END_VAR(_pt, "FeaturePath_LibBasenameStem"); return false; }
    if (len == 0 || len >= out_len) { PROFILE_END_VAR(_pt, "FeaturePath_LibBasenameStem"); return false; }
    memcpy(out, work, len);
    out[len] = '\0';
    PROFILE_END_VAR(_pt, "FeaturePath_LibBasenameStem");
    return true;
}

bool FeaturePath_StemFromCaller(void* caller, char* out, size_t out_len) {
    PROFILE_START(_pt);
    if (!caller || !out || out_len == 0) { PROFILE_END_VAR(_pt, "FeaturePath_StemFromCaller"); return false; }
    out[0] = '\0';
    char mod_path[512];
    if (!Platform_GetModuleOwnerPath(caller, mod_path, sizeof(mod_path))) {
        PROFILE_END_VAR(_pt, "FeaturePath_StemFromCaller");
        return false;
    }
    const char* base = strrchr(mod_path, '\\');
    const char* fs = strrchr(mod_path, '/');
    if (fs && (!base || fs > base)) base = fs;
    base = base ? base + 1 : mod_path;
    char stem[128];
    if (!FeaturePath_LibBasenameStem(base, stem, sizeof(stem))) {
        PROFILE_END_VAR(_pt, "FeaturePath_StemFromCaller");
        return false;
    }
    FeaturePath_SanitizeStem(stem, out, out_len);
    PROFILE_END_VAR(_pt, "FeaturePath_StemFromCaller");
    return out[0] != '\0';
}

bool FeaturePath_ResolveDataPath(void* caller, const char* name, char* out, size_t out_len) {
    PROFILE_START(_pt);
    if (!name || !name[0] || !out || out_len == 0) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
        return false;
    }

    const char* filename = name;
    for (const char* p = name; *p; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    
    if (!filename[0]) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
        return false;
    }

    if (strstr(filename, "..")) { PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath"); return false; }
    
    const char* dot = strrchr(filename, '.');
    size_t stemlen = dot ? (size_t)(dot - filename) : strlen(filename);
    
    char stem[64];
    if (stemlen > 0 && stemlen < 64) {
        int stemok = 1;
        for (size_t i = 0; i < stemlen; i++) {
            char c = filename[i];
            if (c >= 'A' && c <= 'Z') continue;
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') continue;
            stemok = 0;
            break;
        }
        if (stemok) {
            for (size_t i = 0; i < stemlen; i++) {
                char c = filename[i];
                stem[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
            }
            stem[stemlen] = '\0';
            if (Platform_FeatureDataDirExists(stem)) {
                PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
                return Platform_GetFeaturePath(stem, stem, out, out_len);
            }
        }
    } else {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
        return false;
    }
    
    char from_caller[128];
    if (FeaturePath_StemFromCaller(caller, from_caller, sizeof(from_caller))) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
        return Platform_GetFeaturePath(from_caller, stem, out, out_len);
    }
    
    const char* feat = FlintContext_Get()->window.current_feature;
    if (!feat || !feat[0]) { PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath"); return false; }
    char safe[128];
    FeaturePath_SanitizeStem(feat, safe, sizeof(safe));
    if (safe[0] == '\0') { PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath"); return false; }
    
    PROFILE_END_VAR(_pt, "FeaturePath_ResolveDataPath");
    return Platform_GetFeaturePath(safe, stem, out, out_len);
}

bool FeaturePath_ResolveAssetPath(void* caller, const char* name, char* out, size_t out_len) {
    PROFILE_START(_pt);
    if (!name || !name[0] || !out || out_len == 0) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath");
        return false;
    }
    if (!FeaturePath || !FeaturePath[0]) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath");
        return false;
    }
    const char* rel = name;
    while (*rel == '/' || *rel == '\\') rel++;
    if (!rel[0]) { PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath"); return false; }
    for (const char* p = rel; *p; p++) {
        if (*p == '/' || *p == '\\') {
            PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath");
            return false;
        }
    }
    if (strstr(rel, "..")) { PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath"); return false; }
    char stem[128];
    if (!FeaturePath_StemFromCaller(caller, stem, sizeof(stem))) {
        PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath");
        return false;
    }
    PROFILE_END_VAR(_pt, "FeaturePath_ResolveAssetPath");
    return Platform_ResolveAssetFile(stem, rel, out, out_len);
}
