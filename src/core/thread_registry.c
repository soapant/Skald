#include "core/thread_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "core/types/window.h"
#include "platform/platform.h"
#include "deps/log.h"

typedef struct {
  void *ctx;
  char owner[512];
} ThreadEntry;

static ThreadEntry *threads = NULL;
static int thread_count = 0;
static int thread_cap = 0;
static void *thread_mtx = NULL;
static bool thread_mtx_failed = false;

static void thread_reg_lock(void) {
  if (!thread_mtx && !thread_mtx_failed) {
    thread_mtx = Platform_MutexCreate();
    if (!thread_mtx) {
      thread_mtx_failed = true;
      log_error("thread_registry: mutex creation failed; registry will run "
                "unsynchronized");
    }
  }
  if (thread_mtx)
    Platform_MutexLock(thread_mtx);
}

static void thread_reg_unlock(void) {
  if (thread_mtx)
    Platform_MutexUnlock(thread_mtx);
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
  if (thread_count >= thread_cap) {
    int cap = thread_cap > 0 ? thread_cap * 2 : 16;
    ThreadEntry *grown = realloc(threads, (size_t)cap * sizeof(*threads));
    if (!grown) {
      thread_reg_unlock();
      return;
    }
    threads = grown;
    thread_cap = cap;
  }
  threads[thread_count].ctx = ctx;
  threads[thread_count].owner[0] = '\0';
  if (owner_path && owner_path[0])
    snprintf(threads[thread_count].owner, sizeof(threads[thread_count].owner),
             "%s", owner_path);
  thread_count++;
  thread_reg_unlock();
}

void ThreadRegistry_Finish(const void *ctx) {
  if (!ctx)
    return;
  thread_reg_lock();
  for (int i = 0; i < thread_count; i++) {
    if (threads[i].ctx == ctx) {
      threads[i] = threads[thread_count - 1];
      thread_count--;
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
  for (int i = 0; i < thread_count; i++) {
    if (owner_matches(threads[i].owner, owner_path)) {
      thread_reg_unlock();
      return true;
    }
  }
  thread_reg_unlock();
  return false;
}
