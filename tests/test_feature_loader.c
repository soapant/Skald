#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "test.h"

#include "core/feature_loader.h"
#include "core/cap_registry.h"
#include "platform/lib.h"
#include "platform/platform.h"
#include "config_values.h"

extern const char *FeaturePath;

#ifndef TEST_BUILD_DIR
#define TEST_BUILD_DIR "build/tests"
#endif
#define FIXTURE_DIR TEST_BUILD_DIR "/fixture"
#define FIXTURE_NAME "reload_probe"
#define MANIFEST_DIR TEST_BUILD_DIR "/featurepath"

static void remove_hot_artifacts(void) {
  DIR *d = opendir(FIXTURE_DIR);
  if (!d)
    return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (strstr(e->d_name, ".hot") == NULL)
      continue;
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", FIXTURE_DIR, e->d_name);
    remove(path);
  }
  closedir(d);
}

static int has_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  fclose(f);
  return 1;
}

TEST(load_reload_unload) {
  const char *orig_feature_path = FeaturePath;
  FeaturePath = MANIFEST_DIR;
  Platform_MkdirIfNotExists(MANIFEST_DIR);
  Platform_MkdirIfNotExists(FIXTURE_DIR);
  remove_hot_artifacts();

  const char *ext = Platform_LibExtension();
  char fixture[512];
  snprintf(fixture, sizeof(fixture), "%s/%s%s", FIXTURE_DIR, FIXTURE_NAME, ext);
  char hot1[512];
  snprintf(hot1, sizeof(hot1), "%s.hot1", fixture);
  char hot2[512];
  snprintf(hot2, sizeof(hot2), "%s.hot2", hot1);

  ASSERT_TRUE(has_file(fixture));
  ASSERT_FALSE(has_file(hot1));
  ASSERT_FALSE(has_file(hot2));

  bool loaded = feature_loader_load_all_from_dir(FIXTURE_DIR);
  ASSERT_TRUE(loaded);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);
  ASSERT_FALSE(has_file(hot1));

  feature_loader_reload_from(FIXTURE_NAME, fixture);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);
  ASSERT_FALSE(has_file(hot2));

  feature_loader_reload_from(FIXTURE_NAME, fixture);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);
  feature_loader_drain_retired_unloads();
  ASSERT_FALSE(has_file(hot1));
  ASSERT_FALSE(has_file(hot2));

  FeaturePath = orig_feature_path;
  feature_loader_drain_retired_unloads();
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 0);
}

TEST(reload_defers_unload_until_drain) {
  const char *orig_feature_path = FeaturePath;
  FeaturePath = MANIFEST_DIR;
  Platform_MkdirIfNotExists(MANIFEST_DIR);
  Platform_MkdirIfNotExists(FIXTURE_DIR);
  remove_hot_artifacts();
  feature_loader_drain_retired_unloads();

  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 0);

  bool loaded = feature_loader_load_all_from_dir(FIXTURE_DIR);
  ASSERT_TRUE(loaded);
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 0);

  const char *ext = Platform_LibExtension();
  char fixture[512];
  snprintf(fixture, sizeof(fixture), "%s/%s%s", FIXTURE_DIR, FIXTURE_NAME, ext);

  feature_loader_reload_from(FIXTURE_NAME, fixture);
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 1);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);

  feature_loader_reload_from(FIXTURE_NAME, fixture);
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 2);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);

  feature_loader_drain_retired_unloads();
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 0);
  ASSERT_TRUE(cap_resolve_abi("test.reload_probe.cap", CAP_ABI_V1) != NULL);

  feature_loader_drain_retired_unloads();
  ASSERT_EQ_INT(feature_loader_pending_unload_count(), 0);

  FeaturePath = orig_feature_path;
}
int main(void) {
  RUN(load_reload_unload);
  RUN(reload_defers_unload_until_drain);
  return TEST_SUMMARY();
}
