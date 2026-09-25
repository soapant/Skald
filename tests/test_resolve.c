#include "test.h"
#include <sys/stat.h>
#include <stdio.h>
#include "core/context.h"
#include "core/platform.h"

#if defined(_WIN32)
#include <direct.h>
#define TEST_MKDIR(p) _mkdir(p)
#define TEST_RMDIR(p) _rmdir(p)
#define TEST_SEP "\\"
#else
#include <unistd.h>
#define TEST_MKDIR(p) mkdir(p, 0755)
#define TEST_RMDIR(p) rmdir(p)
#define TEST_SEP "/"
#endif

static void test_data_dir(char* out, size_t n) {
#if defined(_WIN32)
    snprintf(out, n, "%s\\AppData\\Roaming\\Flint", Platform_GetRealHome());
#else
    snprintf(out, n, "%s/.local/share/flint", Platform_GetRealHome());
#endif
}

static void test_stem_dir(const char* stem, char* out, size_t n) {
    char base[512];
    test_data_dir(base, sizeof(base));
    snprintf(out, n, "%s%s%s", base, TEST_SEP, stem);
}

TEST(stem_match_null_feature) {
    char dir[512];
    test_stem_dir("chteststem", dir, sizeof(dir));
    TEST_MKDIR(dir);
    FlintContext_Get()->window.current_feature = NULL;
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("chteststem.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "chteststem" TEST_SEP "chteststem.txt") != NULL);
    TEST_RMDIR(dir);
}

TEST(stem_match_stale_feature) {
    char dir[512];
    test_stem_dir("chteststem", dir, sizeof(dir));
    TEST_MKDIR(dir);
    FlintContext_Get()->window.current_feature = (char*)"Todo";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("chteststem.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "chteststem" TEST_SEP "chteststem.txt") != NULL);
    TEST_RMDIR(dir);
}

TEST(existing_behavior_preserved) {
    char dir[512];
    test_stem_dir("todo", dir, sizeof(dir));
    TEST_MKDIR(dir);
    FlintContext_Get()->window.current_feature = (char*)"Todo";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("todo.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "todo" TEST_SEP "todo.txt") != NULL);
    TEST_RMDIR(dir);
}

TEST(unknown_still_fails) {
    FlintContext_Get()->window.current_feature = NULL;
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("nosuchfeaturexyz.txt", out, sizeof(out));
    ASSERT_FALSE(ok);
}

TEST(foreground_first_save) {
    char dir[512];
    test_stem_dir("clipboard_history", dir, sizeof(dir));
    TEST_MKDIR(dir);
    FlintContext_Get()->window.current_feature = (char*)"Clipboard History";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("zzfirstsave.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "clipboard_history" TEST_SEP "zzfirstsave.txt") != NULL);
    TEST_RMDIR(dir);
}

int main(void) {
    RUN(stem_match_null_feature);
    RUN(stem_match_stale_feature);
    RUN(existing_behavior_preserved);
    RUN(unknown_still_fails);
    RUN(foreground_first_save);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
