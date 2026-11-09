#include "core/thread_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "core/types/window.h"
#include "platform/platform.h"

typedef struct {
  void *ctx;
  char owner[512];
} ThreadEntry;

static ThreadEntry *g_threads = NULL;
static int g_thread_count = 0;
static int g_thread_cap = 0;
static void *g_thread_mtx = NULL;

static void thread_reg_lock(void) {
  if (!g_thread_mtx)
    g_thread_mtx = Platform_MutexCreate();
  if (g_thread_mtx)
    Platform_MutexLock(g_thread_mtx);
}

static void thread_reg_unlock(void) {
  if (g_thread_mtx)
    Platform_MutexUnlock(g_thread_mtx);
}

static void basename_of(const char *path, char *out, size_t n) {
  const char *base = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\')
      base = p + 1;
  size_t i = 0;
  for (; base[i] && i + 1 < n; i++)
    out[i] = (char)tolower((unsigned char)base[i]);
  out[i] = '\0';
}

static bool owner_matches(const char *a, const char *b) {
  if (!a[0] || !b[0])
    return false;
  if (strcmp(a, b) == 0)
    return true;
  char ab[256], bb[256];
  basename_of(a, ab, sizeof(ab));
  basename_of(b, bb, sizeof(bb));
  return strcmp(ab, bb) == 0;
}

void ThreadRegistry_Add(void *ctx, const char *owner_path) {
  if (!ctx)
    return;
  thread_reg_lock();
  if (g_thread_count >= g_thread_cap) {
    int cap = g_thread_cap > 0 ? g_thread_cap * 2 : 16;
    ThreadEntry *grown = realloc(g_threads, (size_t)cap * sizeof(*g_threads));
    if (!grown) {
      thread_reg_unlock();
      return;
    }
    g_threads = grown;
    g_thread_cap = cap;
  }
  g_threads[g_thread_count].ctx = ctx;
  g_threads[g_thread_count].owner[0] = '\0';
  if (owner_path && owner_path[0])
    snprintf(g_threads[g_thread_count].owner,
             sizeof(g_threads[g_thread_count].owner), "%s", owner_path);
  g_thread_count++;
  thread_reg_unlock();
}

void ThreadRegistry_Finish(void *ctx) {
  if (!ctx)
    return;
  thread_reg_lock();
  for (int i = 0; i < g_thread_count; i++) {
    if (g_threads[i].ctx == ctx) {
      g_threads[i] = g_threads[g_thread_count - 1];
      g_thread_count--;
      thread_reg_unlock();
      return;
    }
  }
  thread_reg_unlock();
}

bool ThreadRegistry_OwnerActive(const char *owner_path) {
  if (!owner_path || !owner_path[0])
    return false;
  thread_reg_lock();
  for (int i = 0; i < g_thread_count; i++) {
    if (owner_matches(g_threads[i].owner, owner_path)) {
      thread_reg_unlock();
      return true;
    }
  }
  thread_reg_unlock();
  return false;
}
