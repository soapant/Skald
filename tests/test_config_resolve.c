#include "test.h"
#include "config.h"
#include "platform/platform.h"

static void set_all_apps(const char *v) {
  UsedBrowser = v;
  UsedTerminal = v;
  UsedShell = v;
}

TEST(explicit_values_are_never_auto_resolved) {
  set_all_apps("alacritty");
  config_resolve_default_apps();
  ASSERT_FALSE(config_is_auto_resolved_app("UsedTerminal"));
  ASSERT_FALSE(config_is_auto_resolved_app("UsedBrowser"));
  ASSERT_FALSE(config_is_auto_resolved_app("UsedShell"));
  ASSERT_EQ_STR(UsedTerminal, "alacritty");
}

TEST(explicit_values_survive_resolution) {
  set_all_apps("C:/explicit/term.exe");
  config_resolve_default_apps();
  ASSERT_EQ_STR(UsedTerminal, "C:/explicit/term.exe");
  ASSERT_EQ_STR(UsedBrowser, "C:/explicit/term.exe");
}

TEST(linux_style_path_in_config_is_not_flagged_auto) {
  set_all_apps("/usr/bin/foot");
  config_resolve_default_apps();
  ASSERT_FALSE(config_is_auto_resolved_app("UsedTerminal"));
  ASSERT_EQ_STR(UsedTerminal, "/usr/bin/foot");
}

TEST(empty_value_resolves_and_is_flagged) {
  set_all_apps("");
  config_resolve_default_apps();
  const char *resolved = UsedTerminal;
  if (resolved && resolved[0]) {
    ASSERT_TRUE(config_is_auto_resolved_app("UsedTerminal"));
    ASSERT_TRUE(strcmp(resolved, "") != 0);
  } else {
    ASSERT_FALSE(config_is_auto_resolved_app("UsedTerminal"));
  }
}

TEST(auto_flag_clears_when_value_becomes_explicit) {
  set_all_apps("");
  config_resolve_default_apps();
  bool was_auto = config_is_auto_resolved_app("UsedTerminal");
  UsedTerminal = "foot";
  config_resolve_default_apps();
  if (was_auto) {
    ASSERT_FALSE(config_is_auto_resolved_app("UsedTerminal"));
    ASSERT_EQ_STR(UsedTerminal, "foot");
  } else {
    ASSERT_FALSE(config_is_auto_resolved_app("UsedTerminal"));
  }
}

TEST(unknown_names_are_not_auto) {
  ASSERT_FALSE(config_is_auto_resolved_app("UsedFont"));
  ASSERT_FALSE(config_is_auto_resolved_app("COLOR_BG"));
  ASSERT_FALSE(config_is_auto_resolved_app(""));
  ASSERT_FALSE(config_is_auto_resolved_app(NULL));
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
