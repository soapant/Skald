#include "core/flapi.h"


int fl_copy_into_clipboard(const char* text) {
    if (text == NULL) return 0;
    int status = 0;

#if defined(_WIN32)
    size_t len = strlen(text);
    if (len == 0) return 0;

    char* clean_text = (char*)malloc(len + 1);
    if (clean_text == NULL) return 0;
    size_t write_idx = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '\r') {
            clean_text[write_idx++] = text[i];
        }
    }
    clean_text[write_idx] = '\0';

    len = write_idx;
    if (len == 0) {
        free(clean_text);
        return 0;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (hMem == NULL) {
        free(clean_text);
        return 0;
    }

    char* pMem = (char*)GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        free(clean_text);
        return 0;
    }
    
    memcpy(pMem, clean_text, len + 1); 
    GlobalUnlock(hMem);
    free(clean_text);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hMem);
        return 0;
    }

    if (!EmptyClipboard()) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    if (SetClipboardData(CF_TEXT, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }
#else
    char buffer[256];
    char cmd[256];

    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if ((!wayland_display || !wayland_display[0]) || (!runtime_dir || !runtime_dir[0])) {
        static char found_display[64];
        static char found_runtime[256];
        found_display[0] = '\0';
        found_runtime[0] = '\0';
        DIR *dir = opendir("/run/user");
        if (dir) {
            struct dirent *e;
            while ((e = readdir(dir)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char udir[256];
                snprintf(udir, sizeof(udir), "/run/user/%s", e->d_name);
                DIR *sub = opendir(udir);
                if (!sub) continue;
                struct dirent *f;
                while ((f = readdir(sub)) != NULL) {
                    if (strncmp(f->d_name, "wayland-", 8) == 0) {
                        snprintf(found_display, sizeof(found_display), "%s", f->d_name);
                        snprintf(found_runtime, sizeof(found_runtime), "%s", udir);
                        break;
                    }
                }
                closedir(sub);
                if (found_display[0] != '\0') break;
            }
            closedir(dir);
        }
        if (!wayland_display || !wayland_display[0])
            wayland_display = found_display[0] != '\0' ? found_display : NULL;
        if (!runtime_dir || !runtime_dir[0])
            runtime_dir = found_runtime[0] != '\0' ? found_runtime : NULL;
    }

    if (wayland_display != NULL && wayland_display[0] != '\0' &&
        runtime_dir != NULL && runtime_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "XDG_RUNTIME_DIR=%s WAYLAND_DISPLAY=%s wl-copy",
                 runtime_dir, wayland_display);
    } else {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard");
    }

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        pipe = popen("xclip -selection clipboard", "w");
        if (!pipe) {
            pipe = popen("xsel --clipboard --input", "w");
        }
    }

    if (!pipe) {
        fprintf(stderr, "[PLATFORM] Error: Failed to find a valid Linux clipboard manager.\n");
        return 0;
    }

    size_t len = strlen(text);
    size_t max = 8000;  
    if (len > max) {
        fprintf(stderr, "[PLATFORM] Clipboard truncating %zu -> %zu bytes\n", len, max);
        fprintf(pipe, "%.8000s…", text);
    } else {
        fprintf(pipe, "%s", text);
    }
    
    status = pclose(pipe);

#endif
    snprintf(buffer,sizeof(buffer),"Copied %s",text);
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    return (status == 0) ? 1 : 0;
}
