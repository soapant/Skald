#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "test_tmp.h"

#include "core/config_section.h"
#include "core/config_toml.h"

static char PATH[512];

static void section_cleanup(void) { file_remove_path(PATH); }

static bool test_noop_visit(const ConfigTomlEntry *e, void *user) {
  (void)e;
  (void)user;
  return true;
}

static void seed(const char *contents) {
  file_remove_path(PATH);
  FILE *f = fopen(PATH, "wb");
  if (f) {
    fputs(contents, f);
    fclose(f);
  }
}

static char *slurp(void) {
  FILE *f = fopen(PATH, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)n + 1);
  if (buf) {
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    (void)got;
  }
  fclose(f);
  return buf;
}

TEST(set_creates_missing_file) {
  file_remove_path(PATH);
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Term", "Ctrl+Alt+T"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "[hotkeys]") != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"Ctrl+Alt+T\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_appends_inside_existing_section_before_next_section) {
  seed("[other]\nother = \"x\"\n\n[hotkeys]\nA = \"1\"\n\n[color]\nBG = "
       "\"#000\"\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "B", "2"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  const char *color = strstr(s, "[color]");
  const char *b_line = strstr(s, "B = \"2\"");
  const char *a_line = strstr(s, "A = \"1\"");
  ASSERT_TRUE(color != NULL && b_line != NULL && a_line != NULL);
  ASSERT_TRUE(b_line > a_line);
  ASSERT_TRUE(b_line < color);
  free(s);
  file_remove_path(PATH);
}

TEST(set_creates_section_at_eof) {
  seed("[other]\nother = \"x\"\n");
  ASSERT_TRUE(
      config_section_set(PATH, "quicklinks", "YouTube", "https://youtube.com"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "[quicklinks]") != NULL);
  ASSERT_TRUE(strstr(s, "YouTube = \"https://youtube.com\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_upsert_replaces_same_name) {
  seed("[hotkeys]\nTerm = \"Ctrl+Alt+T\"\nOther = \"F\"\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Term", "Super+T"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"Super+T\"") != NULL);
  ASSERT_TRUE(strstr(s, "Ctrl+Alt+T") == NULL);
  ASSERT_TRUE(strstr(s, "Other = \"F\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_preserves_comments_and_entries) {
  seed("# leading comment\n\n[hotkeys]\n# section comment\nTerm = "
       "\"Ctrl+Alt+T\" # entry comment\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "New", "Super+N"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "# leading comment") != NULL);
  ASSERT_TRUE(strstr(s, "# section comment") != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"Ctrl+Alt+T\" # entry comment") != NULL);
  ASSERT_TRUE(strstr(s, "New = \"Super+N\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_replaces_line_with_trailing_comment) {
  seed("[hotkeys]\nTerm = \"Ctrl+Alt+T\" # old comment\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Term", "Super+T"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"Super+T\"") != NULL);
  ASSERT_TRUE(strstr(s, "old comment") == NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_quoted_key_roundtrip) {
  file_remove_path(PATH);
  ASSERT_TRUE(
      config_section_set(PATH, "quicklinks", "My Home", "https://example.com"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "\"My Home\" = \"https://example.com\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(set_malformed_line_refuses_and_keeps_file) {
  seed("[quicklinks]\nbad line without equals\n");
  char *before = slurp();
  ASSERT_FALSE(config_section_set(PATH, "quicklinks", "A", "b"));
  char *after = slurp();
  ASSERT_TRUE(before != NULL && after != NULL);
  ASSERT_EQ_STR(before, after);
  free(before);
  free(after);
  file_remove_path(PATH);
}

TEST(set_duplicate_sections_refuses_and_keeps_file) {
  seed("[quicklinks]\nA = \"1\"\n\n[quicklinks]\nB = \"2\"\n");
  char *before = slurp();
  ASSERT_FALSE(config_section_set(PATH, "quicklinks", "C", "3"));
  char *after = slurp();
  ASSERT_TRUE(before != NULL && after != NULL);
  ASSERT_EQ_STR(before, after);
  free(before);
  free(after);
  file_remove_path(PATH);
}

TEST(set_same_name_in_different_section_keeps_it) {
  seed("[other]\nTerm = \"keepme\"\n\n[hotkeys]\nOther = \"x\"\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Term", "new"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"keepme\"") != NULL);
  ASSERT_TRUE(strstr(s, "Term = \"new\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(clear_removes_entry_and_keeps_section) {
  seed("[hotkeys]\nTerm = \"Ctrl+Alt+T\"\nOther = \"F\"\n");
  ASSERT_TRUE(config_section_clear(PATH, "hotkeys", "Term"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "[hotkeys]") != NULL);
  ASSERT_TRUE(strstr(s, "Term = ") == NULL);
  ASSERT_TRUE(strstr(s, "Other = \"F\"") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(clear_missing_section_is_noop_true) {
  seed("[other]\nother = \"x\"\n");
  ASSERT_TRUE(config_section_clear(PATH, "hotkeys", "Term"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  ASSERT_TRUE(strstr(s, "[other]") != NULL);
  free(s);
  file_remove_path(PATH);
}

TEST(clear_missing_file_is_noop_true) {
  file_remove_path(PATH);
  ASSERT_TRUE(config_section_clear(PATH, "hotkeys", "Term"));
}

TEST(set_then_parse_with_config_toml) {
  seed("[color]\nBG = \"#FFFFFF\"\n\n[hotkeys]\n");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Term", "Ctrl+Alt+T"));
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "Browser", "Super+B"));
  ConfigTomlStatus st =
      config_toml_parse(PATH, (ConfigTomlVisitFn)test_noop_visit, NULL);
  ASSERT_EQ_INT((int)st, (int)CONFIG_TOML_OK);
  file_remove_path(PATH);
}

TEST(set_long_section_name_without_trailing_newline) {
  char section[93];
  memset(section, 'a', sizeof(section) - 1);
  section[sizeof(section) - 1] = '\0';
  seed("x");
  ASSERT_TRUE(config_section_set(PATH, section, "Key", "value"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  if (s) {
    ASSERT_TRUE(strstr(s, section) != NULL);
    ASSERT_TRUE(strstr(s, "Key = \"value\"") != NULL);
    free(s);
  }
  file_remove_path(PATH);
}

TEST(long_section_roundtrips_through_parser) {
  char section[93];
  memset(section, 'a', sizeof(section) - 1);
  section[sizeof(section) - 1] = '\0';
  seed("[short]\nK = \"v\"\n");
  ASSERT_TRUE(config_section_set(PATH, section, "Key", "value"));
  ConfigTomlStatus st =
      config_toml_parse(PATH, (ConfigTomlVisitFn)test_noop_visit, NULL);
  ASSERT_EQ_INT((int)st, (int)CONFIG_TOML_OK);
  file_remove_path(PATH);
}

TEST(set_overlong_section_name_is_rejected) {
  char section[128];
  memset(section, 'b', sizeof(section) - 1);
  section[sizeof(section) - 1] = '\0';
  file_remove_path(PATH);
  ConfigTomlError err;
  ASSERT_EQ_INT(
      (int)config_section_set_ex(&(ConfigSectionSetEx){.path = PATH,
                                                       .section = section,
                                                       .name = "Key",
                                                       .value = "value",
                                                       .err = &err}),
      (int)CONFIG_TOML_ERR_PARSE);
  char *s = slurp();
  ASSERT_TRUE(s == NULL);
  free(s);
}

TEST(set_preserves_last_line_without_newline) {
  seed("[hotkeys]\nA = \"1\"");
  ASSERT_TRUE(config_section_set(PATH, "hotkeys", "B", "2"));
  char *s = slurp();
  ASSERT_TRUE(s != NULL);
  if (s) {
    ASSERT_TRUE(strstr(s, "A = \"1\"") != NULL);
    ASSERT_TRUE(strstr(s, "B = \"2\"") != NULL);
    free(s);
  }
  file_remove_path(PATH);
}

int main(void) {
  test_tmp_path(PATH, sizeof(PATH), "test_config_section.toml");
  TEST_GLOBAL_TEARDOWN(section_cleanup);
  RUN(set_creates_missing_file);
  RUN(set_appends_inside_existing_section_before_next_section);
  RUN(set_creates_section_at_eof);
  RUN(set_upsert_replaces_same_name);
  RUN(set_preserves_comments_and_entries);
  RUN(set_replaces_line_with_trailing_comment);
  RUN(set_quoted_key_roundtrip);
  RUN(set_malformed_line_refuses_and_keeps_file);
  RUN(set_duplicate_sections_refuses_and_keeps_file);
  RUN(set_same_name_in_different_section_keeps_it);
  RUN(clear_removes_entry_and_keeps_section);
  RUN(clear_missing_section_is_noop_true);
  RUN(clear_missing_file_is_noop_true);
  RUN(set_then_parse_with_config_toml);
  RUN(set_long_section_name_without_trailing_newline);
  RUN(long_section_roundtrips_through_parser);
  RUN(set_overlong_section_name_is_rejected);
  RUN(set_preserves_last_line_without_newline);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
