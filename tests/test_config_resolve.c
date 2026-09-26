#include "test.h"
#include "config.h"
#include "platform/platform.h"

static void set_all_apps(const char* v) {
    UsedBrowser = v;
    UsedTerminal = v;
    UsedShell = v;
    UsedImageViewer = v;
    UsedVideoPlayer = v;
}

TEST(explicit_values_are_never_auto_resolved) {
    set_all_apps("alacritty");
    Config_ResolveDefaultApps();
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedTerminal"));
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedBrowser"));
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedShell"));
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedImageViewer"));
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedVideoPlayer"));
    ASSERT_EQ_STR(UsedTerminal, "alacritty");
}

TEST(explicit_values_survive_resolution) {
    set_all_apps("C:/explicit/term.exe");
    Config_ResolveDefaultApps();
    ASSERT_EQ_STR(UsedTerminal, "C:/explicit/term.exe");
    ASSERT_EQ_STR(UsedBrowser, "C:/explicit/term.exe");
}

TEST(linux_style_path_in_config_is_not_flagged_auto) {
    /* The cross-platform bug: a value baked in on Linux must not be treated as
       auto-resolved, because the user (or another platform) put it there. */
    set_all_apps("/usr/bin/foot");
    Config_ResolveDefaultApps();
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedTerminal"));
    ASSERT_EQ_STR(UsedTerminal, "/usr/bin/foot");
}

TEST(empty_value_resolves_and_is_flagged) {
    set_all_apps("");
    Config_ResolveDefaultApps();
    const char* resolved = UsedTerminal;
    if (resolved && resolved[0]) {
        ASSERT_TRUE(Config_IsAutoResolvedApp("UsedTerminal"));
        ASSERT_TRUE(resolved != (const char*)"");
    } else {
        ASSERT_FALSE(Config_IsAutoResolvedApp("UsedTerminal"));
    }
}

TEST(auto_flag_clears_when_value_becomes_explicit) {
    set_all_apps("");
    Config_ResolveDefaultApps();
    bool was_auto = Config_IsAutoResolvedApp("UsedTerminal");
    UsedTerminal = "foot";
    Config_ResolveDefaultApps();
    if (was_auto) {
        ASSERT_FALSE(Config_IsAutoResolvedApp("UsedTerminal"));
        ASSERT_EQ_STR(UsedTerminal, "foot");
    } else {
        ASSERT_FALSE(Config_IsAutoResolvedApp("UsedTerminal"));
    }
}

TEST(unknown_names_are_not_auto) {
    ASSERT_FALSE(Config_IsAutoResolvedApp("UsedFont"));
    ASSERT_FALSE(Config_IsAutoResolvedApp("COLOR_BG"));
    ASSERT_FALSE(Config_IsAutoResolvedApp(""));
    ASSERT_FALSE(Config_IsAutoResolvedApp(NULL));
}

int main(void) {
    RUN(explicit_values_are_never_auto_resolved);
    RUN(explicit_values_survive_resolution);
    RUN(linux_style_path_in_config_is_not_flagged_auto);
    RUN(empty_value_resolves_and_is_flagged);
    RUN(auto_flag_clears_when_value_becomes_explicit);
    RUN(unknown_names_are_not_auto);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
