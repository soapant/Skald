#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "test_tmp.h"

#include "core/config_toml.h"

static char PATH[512];

static void toml_cleanup(void) { file_remove_path(PATH); }

static ConfigTomlEntry entries[32];
static int entry_count;

static bool collecting_visit(const ConfigTomlEntry *e, void *user) {
  (void)user;
  if (entry_count < (int)(sizeof(entries) / sizeof(entries[0])))
    entries[entry_count] = *e;
  entry_count++;
  return true;
}

static bool rejecting_visit(const ConfigTomlEntry *e, void *user) {
  (void)e;
  (void)user;
  return false;
}

static void reset(void) {
  entry_count = 0;
  memset(entries, 0, sizeof(entries));
}

static void seed(const char *contents) {
  file_remove_path(PATH);
  FILE *f = fopen(PATH, "wb");
  if (f) {
    fputs(contents, f);
    fclose(f);
  }
}

TEST(parse_collects_sections_and_value_types) {
  reset();
  seed("[general]\nFlag = true\nCount = -42\nRatio = 1.5\nTitle = "
       "\"hello\"\n");
  ConfigTomlStatus st = config_toml_parse(PATH, collecting_visit, NULL);
  ASSERT_EQ_INT((int)st, (int)CONFIG_TOML_OK);
  ASSERT_EQ_INT(entry_count, 4);
  if (entry_count == 4) {
    ASSERT_EQ_STR(entries[0].section, "general");
    ASSERT_EQ_STR(entries[0].key, "Flag");
    ASSERT_EQ_INT((int)entries[0].type, (int)CONFIG_TOML_BOOL);
    ASSERT_EQ_STR(entries[0].raw, "true");
    ASSERT_EQ_STR(entries[1].key, "Count");
    ASSERT_EQ_INT((int)entries[1].type, (int)CONFIG_TOML_INT);
    ASSERT_EQ_STR(entries[1].raw, "-42");
    ASSERT_EQ_STR(entries[2].key, "Ratio");
    ASSERT_EQ_INT((int)entries[2].type, (int)CONFIG_TOML_FLOAT);
    ASSERT_EQ_STR(entries[2].raw, "1.5");
    ASSERT_EQ_STR(entries[3].key, "Title");
    ASSERT_EQ_INT((int)entries[3].type, (int)CONFIG_TOML_STRING);
    ASSERT_EQ_STR(entries[3].raw, "\"hello\"");
  }
  file_remove_path(PATH);
}

TEST(parse_tracks_section_changes_and_ignores_comments) {
  reset();
  seed("# leading\n[one]\nA = \"1\" # trailing\n\n[two]\nB = \"2\"\n");
  ASSERT_EQ_INT((int)config_toml_parse(PATH, collecting_visit, NULL),
                (int)CONFIG_TOML_OK);
  ASSERT_EQ_INT(entry_count, 2);
  if (entry_count == 2) {
    ASSERT_EQ_STR(entries[0].section, "one");
    ASSERT_EQ_STR(entries[0].key, "A");
    ASSERT_EQ_STR(entries[0].raw, "\"1\"");
    ASSERT_EQ_STR(entries[1].section, "two");
    ASSERT_EQ_STR(entries[1].key, "B");
  }
  file_remove_path(PATH);
}

TEST(parse_accepts_quoted_keys_with_escapes) {
  reset();
  seed("[hotkeys]\n\"My Key\" = \"Ctrl+T\"\n\"line\\nbreak\" = \"x\"\n");
  ASSERT_EQ_INT((int)config_toml_parse(PATH, collecting_visit, NULL),
                (int)CONFIG_TOML_OK);
  ASSERT_EQ_INT(entry_count, 2);
  if (entry_count == 2) {
    ASSERT_EQ_STR(entries[0].key, "My Key");
    ASSERT_TRUE(strchr(entries[1].key, '\n') != NULL);
  }
  file_remove_path(PATH);
}

TEST(parse_accepts_exponent_floats) {
  reset();
  seed("Small = 1.25e-1\nBig = 2E3\nInt = 42\n");
  ASSERT_EQ_INT((int)config_toml_parse(PATH, collecting_visit, NULL),
                (int)CONFIG_TOML_OK);
  ASSERT_EQ_INT(entry_count, 3);
  if (entry_count == 3) {
    ASSERT_EQ_INT((int)entries[0].type, (int)CONFIG_TOML_FLOAT);
    ASSERT_EQ_STR(entries[0].raw, "1.25e-1");
    ASSERT_EQ_INT((int)entries[1].type, (int)CONFIG_TOML_FLOAT);
    ASSERT_EQ_STR(entries[1].raw, "2E3");
    ASSERT_EQ_INT((int)entries[2].type, (int)CONFIG_TOML_INT);
  }
  file_remove_path(PATH);
}

TEST(parse_rejects_malformed_numbers) {
  reset();
  seed("Bad = 1e\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_PARSE);
  file_remove_path(PATH);
}

TEST(parse_unterminated_section_reports_line) {
  reset();
  seed("Key = \"x\"\n[broken\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_PARSE);
  ASSERT_EQ_INT(err.line, 2);
  file_remove_path(PATH);
}

TEST(parse_invalid_section_name_reports_line) {
  reset();
  seed("[has space]\nKey = \"x\"\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_PARSE);
  ASSERT_EQ_INT(err.line, 1);
  file_remove_path(PATH);
}

TEST(parse_missing_equals_reports_line) {
  reset();
  seed("[one]\nBadLine\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_PARSE);
  ASSERT_EQ_INT(err.line, 2);
  file_remove_path(PATH);
}

TEST(parse_unknown_value_type_reports_line) {
  reset();
  seed("[one]\nKey = [1, 2]\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_PARSE);
  ASSERT_EQ_INT(err.line, 2);
  file_remove_path(PATH);
}

TEST(parse_visitor_rejection_reports_rejected) {
  reset();
  seed("[one]\nKey = \"x\"\n");
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, rejecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_REJECTED);
  ASSERT_EQ_INT(err.line, 2);
  file_remove_path(PATH);
}

TEST(parse_missing_file_reports_io_error) {
  file_remove_path(PATH);
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_IO);
  ASSERT_TRUE(err.message[0] != '\0');
}

TEST(parse_null_arguments_report_arg_error) {
  ConfigTomlError err;
  ASSERT_EQ_INT((int)config_toml_parse_ex(NULL, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_ARG);
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, NULL, NULL, &err),
                (int)CONFIG_TOML_ERR_ARG);
}

TEST(status_names_are_stable) {
  ASSERT_EQ_STR(config_toml_status_name(CONFIG_TOML_OK), "ok");
  ASSERT_EQ_STR(config_toml_status_name(CONFIG_TOML_ERR_IO), "io");
  ASSERT_EQ_STR(config_toml_status_name(CONFIG_TOML_ERR_PARSE), "parse");
  ASSERT_EQ_STR(config_toml_status_name(CONFIG_TOML_ERR_ARG), "argument");
  ASSERT_EQ_STR(config_toml_status_name(CONFIG_TOML_ERR_REJECTED), "rejected");
}

TEST(error_state_is_cleared_before_parse) {
  ConfigTomlError err;
  err.status = CONFIG_TOML_ERR_IO;
  err.line = 42;
  strcpy(err.message, "stale");
  file_remove_path(PATH);
  ASSERT_EQ_INT((int)config_toml_parse_ex(PATH, collecting_visit, NULL, &err),
                (int)CONFIG_TOML_ERR_IO);
  ASSERT_EQ_INT(err.line, 0);
  ASSERT_EQ_STR(err.message, "could not open file");
}

int main(void) {
  test_tmp_path(PATH, sizeof(PATH), "test_config_toml.toml");
  TEST_GLOBAL_TEARDOWN(toml_cleanup);
  RUN(parse_collects_sections_and_value_types);
  RUN(parse_tracks_section_changes_and_ignores_comments);
  RUN(parse_accepts_quoted_keys_with_escapes);
  RUN(parse_accepts_exponent_floats);
  RUN(parse_rejects_malformed_numbers);
  RUN(parse_unterminated_section_reports_line);
  RUN(parse_invalid_section_name_reports_line);
  RUN(parse_missing_equals_reports_line);
  RUN(parse_unknown_value_type_reports_line);
  RUN(parse_visitor_rejection_reports_rejected);
  RUN(parse_missing_file_reports_io_error);
  RUN(parse_null_arguments_report_arg_error);
  RUN(status_names_are_stable);
  RUN(error_state_is_cleared_before_parse);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
