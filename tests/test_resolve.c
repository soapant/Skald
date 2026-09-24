#include "test.h"
#include <sys/stat.h>
#include <unistd.h>
#include "core/context.h"
#include "core/platform.h"

TEST(stem_match_null_feature) {
    mkdir("/home/diggy/.local/share/flint/chteststem", 0755);
    FlintContext_Get()->window.current_feature = NULL;
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("chteststem.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "chteststem/chteststem.txt") != NULL);
    rmdir("/home/diggy/.local/share/flint/chteststem");
}

TEST(stem_match_stale_feature) {
    mkdir("/home/diggy/.local/share/flint/chteststem", 0755);
    FlintContext_Get()->window.current_feature = (char*)"Todo";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("chteststem.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "chteststem/chteststem.txt") != NULL);
    rmdir("/home/diggy/.local/share/flint/chteststem");
}

TEST(existing_behavior_preserved) {
    FlintContext_Get()->window.current_feature = (char*)"Todo";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("todo.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "todo/todo.txt") != NULL);
}

TEST(unknown_still_fails) {
    FlintContext_Get()->window.current_feature = NULL;
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("nosuchfeaturexyz.txt", out, sizeof(out));
    ASSERT_FALSE(ok);
}

TEST(foreground_first_save) {
    FlintContext_Get()->window.current_feature = (char*)"Clipboard History";
    char out[512] = {0};
    bool ok = Platform_ResolveDataPath("zzfirstsave.txt", out, sizeof(out));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(strstr(out, "clipboard_history/zzfirstsave.txt") != NULL);
}

int main(void) {
    RUN(stem_match_null_feature);
    RUN(stem_match_stale_feature);
    RUN(existing_behavior_preserved);
    RUN(unknown_still_fails);
    RUN(foreground_first_save);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
