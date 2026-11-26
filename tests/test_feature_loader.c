#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test.h"
#include "test_tmp.h"

#include "core/feature_loader.h"
#include "core/cap_registry.h"
#include "core/skapi.h"
#include "platform/lib.h"
#include "platform/platform.h"
#include "config_values.h"

extern _Atomic(const char *) FeaturePath;

#define FIXTURE_DIR TEST_BUILD_DIR "/fixture"
#define FIXTURE_NAME "reload_probe"
#define MANIFEST_DIR TEST_BUILD_DIR "/featurepath"

static void remove_hot_artifacts(void) {
  DirIter *it = Platform_DirOpen(FIXTURE_DIR);
  if (!it)
    return;
  char name[512];
  bool is_dir = false;
  while (Platform_DirNext(it, name, sizeof(name), &is_dir)) {
    if (strstr(name, ".hot") == NULL)
      continue;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", FIXTURE_DIR, name);
    file_remove_path(path);
  }
  Platform_DirClose(it);
}

static bool has_file(const char *path) { return sk_file_exists(path); }

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
  bool named_reload_ok = feature_loader_reload_from(FIXTURE_NAME, NULL);
  ASSERT_TRUE(named_reload_ok);
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
TEST(baseline_manifest_preserves_declarations) {
  const char *orig_feature_path = FeaturePath;
  FeaturePath = MANIFEST_DIR;
  Platform_MkdirIfNotExists(MANIFEST_DIR);
  char dir[512];
  snprintf(dir, sizeof(dir), "%s/%s", MANIFEST_DIR, FIXTURE_NAME);
  Platform_MkdirIfNotExists(dir);
  char mpath[600];
  snprintf(mpath, sizeof(mpath), "%s/manifest.toml", dir);

  FILE *f = fopen(mpath, "wb");
  if (!f) {
    FeaturePath = orig_feature_path;
    ASSERT_TRUE(false);
    return;
  }
  fputs("name = \"reload_probe\"\ndescription = \"\"\n\n[provides]\n\n"
        "\"test.keep.provided\"\n\n[depends]\n\n\"control_center\"\n",
        f);
  fclose(f);

  feature_loader_load_all_from_dir(FIXTURE_DIR);

  f = fopen(mpath, "rb");
  if (!f) {
    FeaturePath = orig_feature_path;
    ASSERT_TRUE(false);
    return;
  }
  char buf[2048] = {0};
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  buf[n] = '\0';
  ASSERT_TRUE(strstr(buf, "control_center") != NULL);
  ASSERT_TRUE(strstr(buf, "test.keep.provided") != NULL);

  file_remove_path(mpath);
  FeaturePath = orig_feature_path;
}

static const char *s_orig_feature_path;

static void feature_loader_cleanup(void) {
  FeaturePath = s_orig_feature_path;
  test_tmp_remove_tree(MANIFEST_DIR);
  remove_hot_artifacts();
}

int main(void) {
  s_orig_feature_path = FeaturePath;
  TEST_GLOBAL_TEARDOWN(feature_loader_cleanup);
  RUN(load_reload_unload);
  RUN(reload_defers_unload_until_drain);
  RUN(baseline_manifest_preserves_declarations);
  return TEST_SUMMARY();
}
