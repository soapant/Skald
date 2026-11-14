#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test.h"

#include "core/manifest.h"

TEST(trim_strips_surrounding_whitespace) {
  char s1[32];
  strcpy(s1, "   hello   ");
  Manifest_Trim(s1);
  ASSERT_EQ_STR(s1, "hello");

  char s2[64];
  strcpy(s2, "\t  answer\r\n");
  Manifest_Trim(s2);
  ASSERT_EQ_STR(s2, "answer");

  char s3[32];
  strcpy(s3, "\t\t spaced \t\t");
  Manifest_Trim(s3);
  ASSERT_EQ_STR(s3, "spaced");
}

TEST(unquote_strips_comments) {
  char s1[64];
  strcpy(s1, "value # the rest is gone");
  Manifest_Unquote(s1);
  ASSERT_EQ_STR(s1, "value");

  char s2[64];
  strcpy(s2, "\"quoted\" # comment");
  Manifest_Unquote(s2);
  ASSERT_EQ_STR(s2, "quoted");
}

TEST(unquote_strips_matching_quotes) {
  char s1[64];
  strcpy(s1, "\"hello\"");
  Manifest_Unquote(s1);
  ASSERT_EQ_STR(s1, "hello");

  char s2[64];
  strcpy(s2, "'world'");
  Manifest_Unquote(s2);
  ASSERT_EQ_STR(s2, "world");

  char s3[64];
  strcpy(s3, "\"unbalanced");
  Manifest_Unquote(s3);
  ASSERT_EQ_STR(s3, "\"unbalanced");
}

TEST(parse_minimal_manifest) {
  const char *data = "name = \"myfeature\"\n"
                     "description = \"does things\"\n"
                     "\n"
                     "[provides]\n"
                     "mycap.thing\n"
                     "\n"
                     "[depends]\n"
                     "control_center\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("myfeature", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_STR(m.name, "myfeature");
  ASSERT_EQ_INT(m.provide_count, 1);
  ASSERT_EQ_STR(m.provides[0], "mycap.thing");
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "control_center");
}

TEST(parse_name_key_overrides_default) {
  const char *data = "name = \"renamed\"\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("original", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_STR(m.name, "renamed");
}

TEST(parse_crlf_line_endings) {
  const char *data = "name = \"crlf\"\r\n[provides]\r\ncap.one\r\ncap.two\r\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("crlf", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.provide_count, 2);
  ASSERT_EQ_STR(m.provides[0], "cap.one");
  ASSERT_EQ_STR(m.provides[1], "cap.two");
}

TEST(parse_missing_trailing_newline) {
  const char *data = "[depends]\ncontrol_center";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "control_center");
}

TEST(parse_comments_and_blank_lines_ignored) {
  const char *data = "# a comment\n"
                     "\n"
                     "[depends] # trailing comment\n"
                     "control_center # inline comment\n"
                     "# another comment\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "control_center");
}

TEST(parse_provides_before_or_after_depends) {
  const char *data = "[depends]\ndep_a\n[provides]\nprov_a\n[depends]\ndep_b\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 2);
  ASSERT_EQ_STR(m.deps[0], "dep_a");
  ASSERT_EQ_STR(m.deps[1], "dep_b");
  ASSERT_EQ_INT(m.provide_count, 1);
  ASSERT_EQ_STR(m.provides[0], "prov_a");
}

TEST(unquote_keeps_hash_inside_quotes) {
  char s1[64];
  strcpy(s1, "\"value # not a comment\"");
  Manifest_Unquote(s1);
  ASSERT_EQ_STR(s1, "value # not a comment");

  char s2[64];
  strcpy(s2, "'keep # also kept'");
  Manifest_Unquote(s2);
  ASSERT_EQ_STR(s2, "keep # also kept");
}

TEST(parse_quoted_hash_is_not_a_comment) {
  const char *data = "description = \"runs # not a comment\"\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
}

TEST(parse_unterminated_section_is_invalid) {
  const char *data = "[provides\ncap.one\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_FALSE(m.valid);
}

TEST(parse_unterminated_value_quote_is_invalid) {
  const char *data = "description = \"unterminated\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_FALSE(m.valid);
}

TEST(parse_empty_key_is_invalid) {
  const char *data = "= \"no key\"\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_FALSE(m.valid);
}

TEST(parse_binary_garbage_is_invalid) {
  const char *data = "\x01\x02\x03\x04\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_FALSE(m.valid);
}

TEST(parse_unknown_sections_ignored) {
  const char *data = "[other]\nnot_a_dep\n[depends]\nreal_dep\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "real_dep");
}

TEST(parse_key_value_line_resets_section) {
  const char *data = "[depends]\nsome = value\nreal_dep\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 0);
}

TEST(parse_depends_cap_at_max) {
  char data[4096];
  size_t off = 0;
  int written = snprintf(data, sizeof(data), "[depends]\n");
  off += (size_t)written;
  for (int i = 0; i < MANIFEST_LIST_MAX + 5; i++) {
    int written = snprintf(data + off, sizeof(data) - off, "dep_%03d\n", i);
    if (written <= 0) {
      ASSERT_TRUE(false);
      return;
    }
    off += (size_t)written;
  }
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, MANIFEST_LIST_MAX);
}

TEST(parse_empty_data_is_valid_with_zero_counts) {
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", "", &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 0);
  ASSERT_EQ_INT(m.provide_count, 0);
}

TEST(parse_null_data_returns_false) {
  FeatureManifest m;
  ASSERT_FALSE(Manifest_ParseString("x", NULL, &m));
}

TEST(parse_full_baseline_file) {
  const char *data = "name = \"quicklink\"\n"
                     "description = \"\"\n"
                     "\n"
                     "[provides]\n"
                     "\n"
                     "[depends]\n"
                     "control_center\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("quicklink", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_STR(m.name, "quicklink");
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "control_center");
}

TEST(parse_file_missing_returns_false_and_invalid) {
  FeatureManifest m;
  memset(&m, 0xAB, sizeof(m));
  ASSERT_FALSE(Manifest_ParseFile("definitely-not-here-skald-test-abc123.toml",
                                  "x", &m));
  ASSERT_FALSE(m.valid);
  ASSERT_EQ_STR(m.name, "x");
}

TEST(parse_file_roundtrip) {
  const char *path = "build/skald_test_manifest_roundtrip.toml";
  FILE *f = fopen(path, "wb");
  ASSERT_TRUE(f != NULL);
  if (!f) {
    return;
  }
  fputs("name = \"roundtrip\"\n[provides]\ncap.x\n[depends]\ndep.y\n", f);
  fclose(f);

  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseFile(path, "roundtrip", &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_STR(m.name, "roundtrip");
  ASSERT_EQ_INT(m.provide_count, 1);
  ASSERT_EQ_STR(m.provides[0], "cap.x");
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "dep.y");
  remove(path);
}

TEST(parse_comment_inside_depends_would_strip_hash) {
  const char *data = "[depends]\ndep#one\n";
  FeatureManifest m;
  ASSERT_TRUE(Manifest_ParseString("x", data, &m));
  ASSERT_TRUE(m.valid);
  ASSERT_EQ_INT(m.dep_count, 1);
  ASSERT_EQ_STR(m.deps[0], "dep");
}

int main(void) {
  RUN(trim_strips_surrounding_whitespace);
  RUN(unquote_strips_comments);
  RUN(unquote_keeps_hash_inside_quotes);
  RUN(unquote_strips_matching_quotes);
  RUN(parse_minimal_manifest);
  RUN(parse_quoted_hash_is_not_a_comment);
  RUN(parse_unterminated_section_is_invalid);
  RUN(parse_unterminated_value_quote_is_invalid);
  RUN(parse_empty_key_is_invalid);
  RUN(parse_binary_garbage_is_invalid);
  RUN(parse_name_key_overrides_default);
  RUN(parse_crlf_line_endings);
  RUN(parse_missing_trailing_newline);
  RUN(parse_comments_and_blank_lines_ignored);
  RUN(parse_provides_before_or_after_depends);
  RUN(parse_unknown_sections_ignored);
  RUN(parse_key_value_line_resets_section);
  RUN(parse_depends_cap_at_max);
  RUN(parse_empty_data_is_valid_with_zero_counts);
  RUN(parse_null_data_returns_false);
  RUN(parse_full_baseline_file);
  RUN(parse_file_missing_returns_false_and_invalid);
  RUN(parse_file_roundtrip);
  RUN(parse_comment_inside_depends_would_strip_hash);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
