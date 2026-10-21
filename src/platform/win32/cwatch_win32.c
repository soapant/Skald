#include "platform/win32/cwatch_win32.h"
#include "core/timer.h"
#include "deps/log.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CWATCH_MAX_DIRS 64
#define CWATCH_BUF_SIZE 8192
#define CWATCH_QUEUE    32
#define CWATCH_PATH_MAX 320
#define CWATCH_DEDUPE_MS 300

typedef struct {
    HANDLE     dir;
    HANDLE     event;
    OVERLAPPED ov;
    char       buf[CWATCH_BUF_SIZE];
    char       rel[CWATCH_PATH_MAX];
    char       abs[CWATCH_PATH_MAX];
    bool       pending;
} CWatchDir;

typedef struct {
    char path[CWATCH_PATH_MAX];
} CWatchEvent;

static CWatchDir   g_cw_dirs[CWATCH_MAX_DIRS];
static int         g_cw_dir_count = 0;
static char        g_cw_root[CWATCH_PATH_MAX] = {0};
static bool        g_cw_started = false;
static CWatchEvent g_cw_queue[CWATCH_QUEUE];
static int         g_cw_head = 0;
static int         g_cw_tail = 0;
static char        g_cw_last[CWATCH_PATH_MAX] = {0};
static DWORD       g_cw_last_tick = 0;

static void cwatch_push(const char* rel) {
    if (!rel || !rel[0]) return;
    DWORD now = GetTickCount();
    if (strcmp(g_cw_last, rel) == 0 && (now - g_cw_last_tick) < CWATCH_DEDUPE_MS) return;
    strncpy(g_cw_last, rel, sizeof(g_cw_last) - 1);
    g_cw_last[sizeof(g_cw_last) - 1] = '\0';
    g_cw_last_tick = now;
    int next = (g_cw_tail + 1) % CWATCH_QUEUE;
    if (next == g_cw_head) return;
    strncpy(g_cw_queue[g_cw_tail].path, rel, sizeof(g_cw_queue[0].path) - 1);
    g_cw_queue[g_cw_tail].path[sizeof(g_cw_queue[0].path) - 1] = '\0';
    g_cw_tail = next;
}

static bool cwatch_post(CWatchDir* d) {
    if (!d->dir || d->dir == INVALID_HANDLE_VALUE) return false;
    d->ov.hEvent = d->event;
    ResetEvent(d->event);
    DWORD returned = 0;
    BOOL ok = ReadDirectoryChangesW(d->dir, d->buf, sizeof(d->buf), TRUE,
                                     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                                     &returned, &d->ov, NULL);
    if (ok) { d->pending = true; return true; }
    d->pending = (GetLastError() == ERROR_IO_PENDING);
    return d->pending;
}

static bool cwatch_add_dir(const char* abs, const char* rel) {
    if (g_cw_dir_count >= CWATCH_MAX_DIRS) return false;
    CWatchDir* d = &g_cw_dirs[g_cw_dir_count];
    memset(d, 0, sizeof(*d));
    d->dir = CreateFileA(abs, FILE_LIST_DIRECTORY,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (d->dir == INVALID_HANDLE_VALUE) return false;
    d->event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!d->event) { CloseHandle(d->dir); d->dir = NULL; return false; }
    strncpy(d->rel, rel ? rel : "", sizeof(d->rel) - 1);
    strncpy(d->abs, abs, sizeof(d->abs) - 1);
    g_cw_dir_count++;
    if (!cwatch_post(d)) {
        CloseHandle(d->event);
        CloseHandle(d->dir);
        g_cw_dir_count--;
        memset(d, 0, sizeof(*d));
        return false;
    }
    return true;
}

static void cwatch_watch_tree(const char* abs, const char* rel) {
    if (!cwatch_add_dir(abs, rel)) return;
    char pattern[CWATCH_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", abs);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == '.') continue;
        char sub[CWATCH_PATH_MAX];
        char subrel[CWATCH_PATH_MAX];
        snprintf(sub, sizeof(sub), "%s\\%s", abs, fd.cFileName);
        if (rel && rel[0]) snprintf(subrel, sizeof(subrel), "%s\\%s", rel, fd.cFileName);
        else snprintf(subrel, sizeof(subrel), "%s", fd.cFileName);
        cwatch_watch_tree(sub, subrel);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

void win32_cwatch_stop(void) {
    for (int i = 0; i < g_cw_dir_count; i++) {
        CWatchDir* d = &g_cw_dirs[i];
        if (d->pending) {
            CancelIoEx(d->dir, &d->ov);
            GetOverlappedResult(d->dir, &d->ov, NULL, TRUE);
            d->pending = false;
        }
        if (d->event) { CloseHandle(d->event); d->event = NULL; }
        if (d->dir && d->dir != INVALID_HANDLE_VALUE) { CloseHandle(d->dir); d->dir = NULL; }
    }
    g_cw_dir_count = 0;
    g_cw_started = false;
    g_cw_root[0] = '\0';
    g_cw_head = 0;
    g_cw_tail = 0;
    g_cw_last[0] = '\0';
    g_cw_last_tick = 0;
}

static void cwatch_start(const char* root) {
    win32_cwatch_stop();
    if (!root || !root[0]) return;
    cwatch_watch_tree(root, "");
    if (g_cw_dir_count == 0) return;
    g_cw_started = true;
    strncpy(g_cw_root, root, sizeof(g_cw_root) - 1);
    g_cw_root[sizeof(g_cw_root) - 1] = '\0';
}

bool win32_cwatch_active(void) {
    return g_cw_started;
}

static void cwatch_drain_dir(CWatchDir* d) {
    if (!d->pending) return;
    if (WaitForSingleObject(d->event, 0) != WAIT_OBJECT_0) return;
    DWORD bytes = 0;
    if (!GetOverlappedResult(d->dir, &d->ov, &bytes, FALSE)) return;
    d->pending = false;
    if (bytes == 0) { cwatch_post(d); return; }

    /* The buffer is reused by the next read, so it has to be parsed before
       the read is re-armed. */
    BYTE* base = (BYTE*)d->buf;
    while (TRUE) {
        FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)base;
        int len = (int)(fni->FileNameLength / sizeof(WCHAR));
        if (len > CWATCH_PATH_MAX - 1) len = CWATCH_PATH_MAX - 1;
        char name[CWATCH_PATH_MAX];
        WideCharToMultiByte(CP_UTF8, 0, fni->FileName, len, name, sizeof(name) - 1, NULL, NULL);
        name[len] = '\0';
        size_t nlen = strlen(name);
        if (fni->Action == FILE_ACTION_ADDED) {
            /* A new directory has to be watched too. FILE_NOTIFY_INFORMATION
               carries no attributes, so ask the filesystem. */
            char full[CWATCH_PATH_MAX];
            snprintf(full, sizeof(full), "%s\\%s", d->abs, name);
            DWORD attr = GetFileAttributesA(full);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                char sub[CWATCH_PATH_MAX];
                char subrel[CWATCH_PATH_MAX];
                snprintf(sub, sizeof(sub), "%s\\%s", d->abs, name);
                if (d->rel[0]) snprintf(subrel, sizeof(subrel), "%s\\%s", d->rel, name);
                else snprintf(subrel, sizeof(subrel), "%s", name);
                cwatch_watch_tree(sub, subrel);
            }
        } else if (fni->Action == FILE_ACTION_MODIFIED && nlen > 2 &&
                   name[nlen - 2] == '.' && name[nlen - 1] == 'c') {
            char rel[CWATCH_PATH_MAX];
            if (d->rel[0]) snprintf(rel, sizeof(rel), "%s\\%s", d->rel, name);
            else snprintf(rel, sizeof(rel), "%s", name);
            cwatch_push(rel);
        }
        if (!fni->NextEntryOffset) break;
        base += fni->NextEntryOffset;
    }
    cwatch_post(d);
}

char* win32_cwatch_poll(const char* root) {
    if (!g_cw_started || !root || strcmp(g_cw_root, root) != 0) {
        cwatch_start(root);
        return NULL;
    }
    for (int i = 0; i < g_cw_dir_count; i++) cwatch_drain_dir(&g_cw_dirs[i]);
    if (g_cw_head == g_cw_tail) return NULL;
    char* out = strdup(g_cw_queue[g_cw_head].path);
    g_cw_head = (g_cw_head + 1) % CWATCH_QUEUE;
    return out;
}
