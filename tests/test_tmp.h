#ifndef SKALD_TEST_TMP_H
#define SKALD_TEST_TMP_H

#include <stdbool.h>
#include <stdio.h>

#include "helpers/helpers_fs.h"
#include "platform/platform.h"

#ifndef TEST_BUILD_DIR
#define TEST_BUILD_DIR "build/tests"
#endif

static inline void test_tmp_path(char *out, size_t cap, const char *name) {
  snprintf(out, cap, "%s/%s", TEST_BUILD_DIR, name);
}

static inline bool test_tmp_make(const char *path) {
  return Platform_MkdirIfNotExists(path);
}

static inline void test_tmp_remove_tree(const char *path) {
  DirIter *it = Platform_DirOpen(path);
  if (it) {
    char name[512];
    bool is_dir = false;
    while (Platform_DirNext(it, name, sizeof(name), &is_dir)) {
      char child[1024];
      snprintf(child, sizeof(child), "%s/%s", path, name);
      if (is_dir)
        test_tmp_remove_tree(child);
      else
        file_remove_path(child);
    }
    Platform_DirClose(it);
  }
  Platform_RemoveDirectory(path);
}

#endif
