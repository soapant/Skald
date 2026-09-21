#include "core/remap_manager.h"
#include "core/types/keys.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "deps/log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>

#define MAX_REMAPS 64
#define MAX_SRC_VKS 8
#define REMAP_MAX_WINDOWS 8

typedef struct {
    int src_mods;
    int src_vks[MAX_SRC_VKS];
    int src_vk_count;
    int dst_mods;
    int dst_vk;
    bool active;
    bool whitelist;
    char* windows[REMAP_MAX_WINDOWS];
    int window_count;
} RemapBinding;

static RemapBinding g_remaps[MAX_REMAPS];
static int g_remap_count = 0;

static int g_phys_vk[256] = {0};
static int g_virt_vk[256] = {0};

static int vk_from_token(const char* tok) {
    if (!tok || !tok[0]) return 0;
    if (strlen(tok) == 1) {
        char c = tok[0];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return toupper((unsigned char)c);
        if (c >= '0' && c <= '9') return c;
        if (c == ' ') return VK_SPACE;
    }
    if (strcasecmp(tok, "Space") == 0) return VK_SPACE;
    if (strcasecmp(tok, "Tab") == 0) return VK_TAB;
    if (strcasecmp(tok, "Enter") == 0 || strcasecmp(tok, "Return") == 0) return VK_RETURN;
    if (strcasecmp(tok, "Escape") == 0 || strcasecmp(tok, "Esc") == 0) return VK_ESCAPE;
    if (strcasecmp(tok, "Backspace") == 0 || strcasecmp(tok, "Back") == 0) return VK_BACK;
    if (strcasecmp(tok, "Delete") == 0 || strcasecmp(tok, "Del") == 0) return VK_DELETE;
    if (strcasecmp(tok, "Up") == 0 || strcasecmp(tok, "ArrowUp") == 0) return VK_UP;
    if (strcasecmp(tok, "Down") == 0 || strcasecmp(tok, "ArrowDown") == 0) return VK_DOWN;
    if (strcasecmp(tok, "Left") == 0 || strcasecmp(tok, "ArrowLeft") == 0) return VK_LEFT;
    if (strcasecmp(tok, "Right") == 0 || strcasecmp(tok, "ArrowRight") == 0) return VK_RIGHT;
    if (strcasecmp(tok, "Home") == 0) return VK_HOME;
    if (strcasecmp(tok, "End") == 0) return VK_END;
    if (strcasecmp(tok, "PageUp") == 0 || strcasecmp(tok, "Prior") == 0) return VK_PRIOR;
    if (strcasecmp(tok, "PageDown") == 0 || strcasecmp(tok, "Next") == 0) return VK_NEXT;
    if (strcasecmp(tok, "Grave") == 0 || strcasecmp(tok, "`") == 0 || strcasecmp(tok, "Tilde") == 0 || strcasecmp(tok, "~") == 0) return VK_OEM_3;
    if (strcasecmp(tok, "Minus") == 0 || strcasecmp(tok, "-") == 0 || strcasecmp(tok, "OEM_MINUS") == 0) return VK_OEM_MINUS;
    if (strcasecmp(tok, "Plus") == 0 || strcasecmp(tok, "=") == 0 || strcasecmp(tok, "+") == 0 || strcasecmp(tok, "OEM_PLUS") == 0) return VK_OEM_PLUS;
    if (strcasecmp(tok, "Semicolon") == 0 || strcasecmp(tok, ";") == 0 || strcasecmp(tok, ":") == 0 || strcasecmp(tok, "OEM_1") == 0) return VK_OEM_1;
    if (strcasecmp(tok, "Slash") == 0 || strcasecmp(tok, "/") == 0 || strcasecmp(tok, "?") == 0 || strcasecmp(tok, "OEM_2") == 0) return VK_OEM_2;
    if (strcasecmp(tok, "LeftBracket") == 0 || strcasecmp(tok, "[") == 0 || strcasecmp(tok, "{") == 0 || strcasecmp(tok, "OEM_4") == 0) return VK_OEM_4;
    if (strcasecmp(tok, "RightBracket") == 0 || strcasecmp(tok, "]") == 0 || strcasecmp(tok, "}") == 0 || strcasecmp(tok, "OEM_6") == 0) return VK_OEM_6;
    if (strcasecmp(tok, "Quote") == 0 || strcasecmp(tok, "'") == 0 || strcasecmp(tok, "\"") == 0 || strcasecmp(tok, "OEM_7") == 0) return VK_OEM_7;
    if (strcasecmp(tok, "Comma") == 0 || strcasecmp(tok, ",") == 0 || strcasecmp(tok, "<") == 0 || strcasecmp(tok, "OEM_COMMA") == 0) return VK_OEM_COMMA;
    if (strcasecmp(tok, "Period") == 0 || strcasecmp(tok, ".") == 0 || strcasecmp(tok, ">") == 0 || strcasecmp(tok, "OEM_PERIOD") == 0) return VK_OEM_PERIOD;
    if (strcasecmp(tok, "Backslash") == 0 || strcasecmp(tok, "\\") == 0 || strcasecmp(tok, "Pipe") == 0 || strcasecmp(tok, "|") == 0 || strcasecmp(tok, "OEM_5") == 0) return 0xDC;
    if (strcasecmp(tok, "F1") == 0) return VK_F1;
    if (strcasecmp(tok, "F2") == 0) return VK_F1+1;
    if (strcasecmp(tok, "F3") == 0) return VK_F1+2;
    if (strcasecmp(tok, "F4") == 0) return VK_F1+3;
    if (strcasecmp(tok, "F5") == 0) return VK_F1+4;
    if (strcasecmp(tok, "F6") == 0) return VK_F1+5;
    if (strcasecmp(tok, "F7") == 0) return VK_F1+6;
    if (strcasecmp(tok, "F8") == 0) return VK_F1+7;
    if (strcasecmp(tok, "F9") == 0) return VK_F1+8;
    if (strcasecmp(tok, "F10") == 0) return VK_F1+9;
    if (strcasecmp(tok, "F11") == 0) return VK_F1+10;
    if (strcasecmp(tok, "F12") == 0) return VK_F1+11;
    if (strlen(tok) == 1) return toupper((unsigned char)tok[0]);
    return 0;
}

static bool parse_mods_and_vks(const char* str, int* out_mods, int* out_vks, int* out_count) {
    *out_mods = 0;
    *out_count = 0;
    if (!str) return false;
    char buf[256];
    strncpy(buf, str, sizeof(buf)-1);
    buf[sizeof(buf)-1]='\0';
    char* token = strtok(buf, "+");
    while (token) {
        while (*token==' '||*token=='\t') token++;
        char* end = token + strlen(token)-1;
        while (end>token && (*end==' '||*end=='\t')) { *end='\0'; end--; }
        if (strcasecmp(token, "Ctrl")==0 || strcasecmp(token, "Control")==0) {
            *out_mods |= HOTKEY_MOD_CTRL;
        } else if (strcasecmp(token, "Shift")==0) {
            *out_mods |= HOTKEY_MOD_SHIFT;
        } else if (strcasecmp(token, "Alt")==0) {
            *out_mods |= HOTKEY_MOD_ALT;
        } else if (strcasecmp(token, "Win")==0 || strcasecmp(token, "Super")==0 || strcasecmp(token, "Meta")==0) {
            *out_mods |= HOTKEY_MOD_SUPER;
        } else if (strcasecmp(token, "LeftCtrl")==0 || strcasecmp(token, "LCtrl")==0 || strcasecmp(token, "LControl")==0) {
            out_vks[(*out_count)++] = VK_LCONTROL;
        } else if (strcasecmp(token, "RightCtrl")==0 || strcasecmp(token, "RCtrl")==0 || strcasecmp(token, "RControl")==0) {
            out_vks[(*out_count)++] = VK_RCONTROL;
        } else if (strcasecmp(token, "LeftShift")==0 || strcasecmp(token, "LShift")==0) {
            out_vks[(*out_count)++] = VK_LSHIFT;
        } else if (strcasecmp(token, "RightShift")==0 || strcasecmp(token, "RShift")==0) {
            out_vks[(*out_count)++] = VK_RSHIFT;
        } else if (strcasecmp(token, "LeftAlt")==0 || strcasecmp(token, "LAlt")==0 || strcasecmp(token, "LMenu")==0) {
            out_vks[(*out_count)++] = VK_LMENU;
        } else if (strcasecmp(token, "RightAlt")==0 || strcasecmp(token, "RAlt")==0 || strcasecmp(token, "RMenu")==0) {
            out_vks[(*out_count)++] = VK_RMENU;
        } else if (strcasecmp(token, "LeftSuper")==0 || strcasecmp(token, "LWin")==0 || strcasecmp(token, "LSuper")==0) {
            out_vks[(*out_count)++] = VK_LWIN;
        } else if (strcasecmp(token, "RightSuper")==0 || strcasecmp(token, "RWin")==0 || strcasecmp(token, "RSuper")==0) {
            out_vks[(*out_count)++] = VK_RWIN;
        } else {
            int vk = 0;
            bool need_shift = false;
            if (strlen(token) == 1) {
                char c = token[0];
                switch (c) {
                    case '!': vk = '1'; need_shift = true; break;
                    case '@': vk = '2'; need_shift = true; break;
                    case '#': vk = '3'; need_shift = true; break;
                    case '$': vk = '4'; need_shift = true; break;
                    case '%': vk = '5'; need_shift = true; break;
                    case '^': vk = '6'; need_shift = true; break;
                    case '&': vk = '7'; need_shift = true; break;
                    case '*': vk = '8'; need_shift = true; break;
                    case '(': vk = '9'; need_shift = true; break;
                    case ')': vk = '0'; need_shift = true; break;
                    case '_': vk = VK_OEM_MINUS; need_shift = true; break;
                    case '+': vk = VK_OEM_PLUS; need_shift = true; break;
                    case '~': vk = VK_OEM_3; need_shift = true; break;
                    case '`': vk = VK_OEM_3; need_shift = false; break;
                    case '{': vk = VK_OEM_4; need_shift = true; break;
                    case '}': vk = VK_OEM_6; need_shift = true; break;
                    case ':': vk = VK_OEM_1; need_shift = true; break;
                    case '"': vk = VK_OEM_7; need_shift = true; break;
                    case '<': vk = VK_OEM_COMMA; need_shift = true; break;
                    case '>': vk = VK_OEM_PERIOD; need_shift = true; break;
                    case '?': vk = VK_OEM_2; need_shift = true; break;
                    case '|': vk = 0xDC; need_shift = true; break;
                    case '\\': vk = 0xDC; need_shift = false; break;
                    default: vk = 0; break;
                }
            }
            if (vk) {
                if (need_shift) *out_mods |= HOTKEY_MOD_SHIFT;
                if (*out_count < MAX_SRC_VKS) out_vks[(*out_count)++] = vk;
            } else {
                vk = vk_from_token(token);
                if (vk) {
                    if (*out_count < MAX_SRC_VKS) out_vks[(*out_count)++] = vk;
                }
            }
        }
        token = strtok(NULL, "+");
    }
    return true;
}

bool remap_has_bindings(void) {
  return g_remap_count>0; 
}

static bool remap_window_eq(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static bool remap_window_allowed(RemapBinding* b, const char* title) {
    if (!b) return true;
    if (b->window_count == 0) return !b->whitelist;
    if (!b->whitelist) {
        for (int j = 0; j < b->window_count; j++) {
            if (b->windows[j] && b->windows[j][0] && Platform_StringContainsCaseInsensitive(title, b->windows[j])) return false;
        }
        return true;
    }
    if (!title) return false;
    for (int j = 0; j < b->window_count; j++) {
        if (b->windows[j] && b->windows[j][0] && Platform_StringContainsCaseInsensitive(title, b->windows[j])) return true;
    }
    return false;
}

bool remap_register_internal(const char* src, const char* dst, bool whitelist, const char** windows) {
    if (!src || !dst) return false;
    if (g_remap_count >= MAX_REMAPS) return false;
    RemapBinding b = {0};
    b.whitelist = whitelist;
    if (windows) {
        for (int i = 0; windows[i] != NULL && b.window_count < REMAP_MAX_WINDOWS; i++) {
            if (!windows[i][0]) continue;
            char* copy = strdup(windows[i]);
            if (!copy) continue;
            b.windows[b.window_count++] = copy;
        }
    }
    int tmp_vks[MAX_SRC_VKS];
    int tmp_count=0;
    int tmp_mods=0;
    parse_mods_and_vks(src, &tmp_mods, tmp_vks, &tmp_count);
    b.src_mods = tmp_mods;
    b.src_vk_count = tmp_count;
    for (int i=0;i<tmp_count;i++) b.src_vks[i]=tmp_vks[i];

    int dst_mods=0;
    int dst_vks[MAX_SRC_VKS];
    int dst_count=0;
    parse_mods_and_vks(dst, &dst_mods, dst_vks, &dst_count);
    b.dst_mods = dst_mods;
    if (dst_count>0) b.dst_vk = dst_vks[0];
    else b.dst_vk = 0;
    for (int i=0;i<g_remap_count;i++) {
        RemapBinding* e=&g_remaps[i];
        if (e->src_mods==b.src_mods && e->dst_mods==b.dst_mods && e->dst_vk==b.dst_vk && e->src_vk_count==b.src_vk_count && e->whitelist==b.whitelist && e->window_count==b.window_count) {
            bool same=true;
            for (int k=0;k<b.src_vk_count;k++) if (e->src_vks[k]!=b.src_vks[k]) { same=false; break; }
            for (int k=0;k<b.window_count && same;k++) if (!remap_window_eq(e->windows[k], b.windows[k])) same=false;
            if (same) {
                log_warn("remap duplicate ignored src='%s' dst='%s' total=%d", src, dst, g_remap_count);
                for (int k=0;k<b.window_count;k++) free(b.windows[k]);
                return true;
            }
        }
    }
    g_remaps[g_remap_count++] = b;
    log_warn("remap registered src='%s' (mods=%d count=%d) dst='%s' (mods=%d vk=%d) wl=%d wins=%d total=%d", src, tmp_mods, tmp_count, dst, dst_mods, b.dst_vk, b.whitelist ? 1 : 0, b.window_count, g_remap_count);
    return true;
}

void remap_update_phys(int vk_code, int value) {
    if (vk_code<0 || vk_code>=256) return;
    if (value==2) return;
    g_phys_vk[vk_code] = (value != 0);
}

void remap_reset(void) {
    for (int i=0;i<256;i++) {
        if (g_virt_vk[i]) {
            Platform_EmitKey(i, 0);
            g_virt_vk[i]=0;
        }
        g_phys_vk[i]=0;
    }
}

int remap_current_mods(void) {
    int m=0;
    if (g_phys_vk[VK_LCONTROL] || g_phys_vk[VK_RCONTROL] || g_phys_vk[VK_CONTROL]) m|=HOTKEY_MOD_CTRL;
    if (g_phys_vk[VK_LMENU] || g_phys_vk[VK_RMENU] || g_phys_vk[VK_MENU]) m|=HOTKEY_MOD_ALT;
    if (g_phys_vk[VK_LSHIFT] || g_phys_vk[VK_RSHIFT] || g_phys_vk[VK_SHIFT]) m|=HOTKEY_MOD_SHIFT;
    if (g_phys_vk[VK_LWIN] || g_phys_vk[VK_RWIN]) m|=HOTKEY_MOD_SUPER;
    return m;
}

bool remap_is_virt_held(int vk) {
    if (vk<0||vk>=256) return false;
    return g_virt_vk[vk];
}

static bool binding_active(RemapBinding* b) {
    if (b->src_mods==0 && b->src_vk_count==0) return false;
    if (b->src_mods & HOTKEY_MOD_CTRL) { if (! (g_phys_vk[VK_LCONTROL] || g_phys_vk[VK_RCONTROL])) return false; }
    if (b->src_mods & HOTKEY_MOD_ALT) { if (! (g_phys_vk[VK_LMENU] || g_phys_vk[VK_RMENU])) return false; }
    if (b->src_mods & HOTKEY_MOD_SHIFT) { if (! (g_phys_vk[VK_LSHIFT] || g_phys_vk[VK_RSHIFT])) return false; }
    if (b->src_mods & HOTKEY_MOD_SUPER) { if (! (g_phys_vk[VK_LWIN] || g_phys_vk[VK_RWIN])) return false; }
    for (int i=0;i<b->src_vk_count;i++) {
        int vk = b->src_vks[i];
        if (vk<0||vk>=256) return false;
        if (!g_phys_vk[vk]) return false;
    }
    return true;
}

bool remap_handle_vk(int vk_code, int value) {
    if (vk_code<0 || vk_code>=256) return false;
    if (value==2) return false;
    bool active_before[64]={0};
    for (int i=0;i<g_remap_count;i++) active_before[i]=binding_active(&g_remaps[i]);

    remap_update_phys(vk_code, value);

    bool active_after[64]={0};
    for (int i=0;i<g_remap_count;i++) active_after[i]=binding_active(&g_remaps[i]);

    bool allowed[64];
    for (int i=0;i<g_remap_count;i++) allowed[i]=true;
    bool need_title = false;
    for (int i=0;i<g_remap_count;i++) {
        if (g_remaps[i].window_count > 0 && (active_before[i] || active_after[i])) { need_title = true; break; }
    }
    const char* title = NULL;
    if (need_title) title = Platform_GetActiveWindowTitle();
    for (int i=0;i<g_remap_count;i++) {
        if (g_remaps[i].window_count > 0) allowed[i]=remap_window_allowed(&g_remaps[i], title);
    }

    for (int i=0;i<g_remap_count;i++) {
        RemapBinding* b=&g_remaps[i];
        bool before = active_before[i];
        bool after = active_after[i];
        if (!before && after && allowed[i]) {
            for (int k=0;k<b->src_vk_count;k++) {
                int sv = b->src_vks[k];
                if (sv != vk_code) {
                    Platform_EmitKey(sv, 0);
                    g_virt_vk[sv]=0;
                }
            }
            if (b->src_mods & HOTKEY_MOD_CTRL) {
                if (g_phys_vk[VK_LCONTROL]) { Platform_EmitKey(VK_LCONTROL, 0); g_virt_vk[VK_LCONTROL]=0; }
                if (g_phys_vk[VK_RCONTROL]) { Platform_EmitKey(VK_RCONTROL, 0); g_virt_vk[VK_RCONTROL]=0; }
            }
            if (b->src_mods & HOTKEY_MOD_ALT) {
                if (g_phys_vk[VK_LMENU]) { Platform_EmitKey(VK_LMENU, 0); g_virt_vk[VK_LMENU]=0; }
                if (g_phys_vk[VK_RMENU]) { Platform_EmitKey(VK_RMENU, 0); g_virt_vk[VK_RMENU]=0; }
            }
            if (b->src_mods & HOTKEY_MOD_SHIFT) {
                if (g_phys_vk[VK_LSHIFT]) { Platform_EmitKey(VK_LSHIFT, 0); g_virt_vk[VK_LSHIFT]=0; }
                if (g_phys_vk[VK_RSHIFT]) { Platform_EmitKey(VK_RSHIFT, 0); g_virt_vk[VK_RSHIFT]=0; }
            }
            if (b->src_mods & HOTKEY_MOD_SUPER) {
                if (g_phys_vk[VK_LWIN]) { Platform_EmitKey(VK_LWIN, 0); g_virt_vk[VK_LWIN]=0; }
                if (g_phys_vk[VK_RWIN]) { Platform_EmitKey(VK_RWIN, 0); g_virt_vk[VK_RWIN]=0; }
            }
            if (b->dst_mods & HOTKEY_MOD_CTRL) { Platform_EmitKey(VK_LCONTROL, 1); g_virt_vk[VK_LCONTROL]=1; }
            if (b->dst_mods & HOTKEY_MOD_SHIFT) { Platform_EmitKey(VK_LSHIFT, 1); g_virt_vk[VK_LSHIFT]=1; }
            if (b->dst_mods & HOTKEY_MOD_ALT) { Platform_EmitKey(VK_LMENU, 1); g_virt_vk[VK_LMENU]=1; }
            if (b->dst_mods & HOTKEY_MOD_SUPER) { Platform_EmitKey(VK_LWIN, 1); g_virt_vk[VK_LWIN]=1; }
            if (b->dst_vk) { Platform_EmitKey(b->dst_vk, 1); g_virt_vk[b->dst_vk]=1; }
        } else if (before && !after) {
            if (b->dst_vk && g_virt_vk[b->dst_vk]) { Platform_EmitKey(b->dst_vk, 0); g_virt_vk[b->dst_vk]=0; }
            if ((b->dst_mods & HOTKEY_MOD_CTRL) && g_virt_vk[VK_LCONTROL]) { Platform_EmitKey(VK_LCONTROL, 0); g_virt_vk[VK_LCONTROL]=0; }
            if ((b->dst_mods & HOTKEY_MOD_SHIFT) && g_virt_vk[VK_LSHIFT]) { Platform_EmitKey(VK_LSHIFT, 0); g_virt_vk[VK_LSHIFT]=0; }
            if ((b->dst_mods & HOTKEY_MOD_ALT) && g_virt_vk[VK_LMENU]) { Platform_EmitKey(VK_LMENU, 0); g_virt_vk[VK_LMENU]=0; }
            if ((b->dst_mods & HOTKEY_MOD_SUPER) && g_virt_vk[VK_LWIN]) { Platform_EmitKey(VK_LWIN, 0); g_virt_vk[VK_LWIN]=0; }
            if (!allowed[i]) continue;
            if (b->src_mods & HOTKEY_MOD_CTRL) {
                if (g_phys_vk[VK_LCONTROL]) { Platform_EmitKey(VK_LCONTROL, 1); g_virt_vk[VK_LCONTROL]=1; }
                if (g_phys_vk[VK_RCONTROL]) { Platform_EmitKey(VK_RCONTROL, 1); g_virt_vk[VK_RCONTROL]=1; }
            }
            if (b->src_mods & HOTKEY_MOD_ALT) {
                if (g_phys_vk[VK_LMENU]) { Platform_EmitKey(VK_LMENU, 1); g_virt_vk[VK_LMENU]=1; }
                if (g_phys_vk[VK_RMENU]) { Platform_EmitKey(VK_RMENU, 1); g_virt_vk[VK_RMENU]=1; }
            }
            if (b->src_mods & HOTKEY_MOD_SHIFT) {
                if (g_phys_vk[VK_LSHIFT]) { Platform_EmitKey(VK_LSHIFT, 1); g_virt_vk[VK_LSHIFT]=1; }
                if (g_phys_vk[VK_RSHIFT]) { Platform_EmitKey(VK_RSHIFT, 1); g_virt_vk[VK_RSHIFT]=1; }
            }
            if (b->src_mods & HOTKEY_MOD_SUPER) {
                if (g_phys_vk[VK_LWIN]) { Platform_EmitKey(VK_LWIN, 1); g_virt_vk[VK_LWIN]=1; }
                if (g_phys_vk[VK_RWIN]) { Platform_EmitKey(VK_RWIN, 1); g_virt_vk[VK_RWIN]=1; }
            }
        }
    }

    bool swallow = false;
    for (int i=0;i<g_remap_count;i++) {
        RemapBinding* b=&g_remaps[i];
        bool before = active_before[i] && allowed[i];
        bool after = active_after[i] && allowed[i];
        for (int k=0;k<b->src_vk_count;k++) if (b->src_vks[k]==vk_code) {
            if (before || after) swallow = true;
        }
        if (b->src_mods & HOTKEY_MOD_CTRL && (vk_code==VK_LCONTROL||vk_code==VK_RCONTROL||vk_code==VK_CONTROL)) {
            if (before || after) swallow = true;
        }
        if (b->src_mods & HOTKEY_MOD_ALT && (vk_code==VK_LMENU||vk_code==VK_RMENU||vk_code==VK_MENU)) {
            if (before || after) swallow = true;
        }
        if (b->src_mods & HOTKEY_MOD_SHIFT && (vk_code==VK_LSHIFT||vk_code==VK_RSHIFT||vk_code==VK_SHIFT)) {
            if (before || after) swallow = true;
        }
        if (b->src_mods & HOTKEY_MOD_SUPER && (vk_code==VK_LWIN||vk_code==VK_RWIN)) {
            if (before || after) swallow = true;
        }
        if (after && !before) {
            if (b->src_vk_count>0 && b->src_vks[b->src_vk_count-1]==vk_code) swallow=true;
        }
    }
    return swallow;
}

bool remap_should_swallow_vk(int vk_code, int value) {
    return remap_handle_vk(vk_code, value);
}

#if !defined(_WIN32)
bool remap_should_swallow_linux_key(int linux_code, int value) {
    int vk = ConvertLinuxKeyToWindowsVK(linux_code);
    if (!vk) {
        return false;
    }
    return remap_handle_vk(vk, value);
}
#else
bool remap_should_swallow_linux_key(int linux_code, int value) {
    (void)linux_code; (void)value;
    return false;
}
#endif
