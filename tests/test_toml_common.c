#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/config_toml.h"
#include "core/toml_common.h"

TEST(trim_left_and_right) {
  ASSERT_EQ_STR(toml_trim_left("  value  "), "value  ");
  ASSERT_EQ_INT((int)toml_trim_right_len("value  ", 7), 5);
  char buf[32] = "  value\t\r\n";
  toml_trim(buf);
  ASSERT_EQ_STR(buf, "value");
}

TEST(bare_key_validation) {
  ASSERT_TRUE(toml_is_bare_key("FontSize"));
  ASSERT_TRUE(toml_is_bare_key("my-key_2"));
  ASSERT_FALSE(toml_is_bare_key(NULL));
  ASSERT_FALSE(toml_is_bare_key(""));
  ASSERT_FALSE(toml_is_bare_key("a.b"));
  ASSERT_FALSE(toml_is_bare_key("a b"));
}

TEST(strip_comment_keeps_hashes_inside_strings) {
  char out[128];
  size_t n = toml_strip_comment("Title = \"a#b\" # note", out, sizeof(out));
  ASSERT_EQ_INT((int)n, (int)strlen("Title = \"a#b\" "));
  ASSERT_EQ_STR(out, "Title = \"a#b\" ");
}

TEST(strip_comment_handles_escaped_quote) {
  char out[128];
  toml_strip_comment("Title = \"a\\\"#b\" # note", out, sizeof(out));
  ASSERT_EQ_STR(out, "Title = \"a\\\"#b\" ");
}

TEST(quote_balanced_and_control_chars) {
  ASSERT_TRUE(toml_quote_balanced("\"ok\""));
  ASSERT_FALSE(toml_quote_balanced("\"bad"));
  ASSERT_TRUE(toml_quote_balanced("'ok'"));
  ASSERT_FALSE(toml_has_control_char("plain text\t"));
  ASSERT_TRUE(toml_has_control_char("bad\1text"));
}

TEST(unquote_strips_quotes_comments_and_space) {
  char a[64] = "  \"value\" # note  ";
  toml_unquote(a);
  ASSERT_EQ_STR(a, "value");
  char b[64] = "  'other'";
  toml_unquote(b);
  ASSERT_EQ_STR(b, "other");
}

TEST(decode_string_handles_escapes) {
  char out[128];
  ASSERT_TRUE(toml_decode_string("\"a\\nb\\tc\\r\\\"\\\\\"", out, sizeof(out)));
  ASSERT_EQ_STR(out, "a\nb\tc\r\"\\");
  ASSERT_FALSE(toml_decode_string("unquoted", out, sizeof(out)));
  ASSERT_FALSE(toml_decode_string("\"bad\\q\"", out, sizeof(out)));
  ASSERT_FALSE(toml_decode_string(NULL, out, sizeof(out)));
  ASSERT_FALSE(toml_decode_string("\"x\"", NULL, sizeof(out)));
  ASSERT_FALSE(toml_decode_string("\"x\"", out, 0));
}

TEST(encode_string_roundtrip) {
  const char *src = "a\\b\"c\nd\te\rf";
  char enc[128], dec[128];
  ASSERT_TRUE(toml_encode_string(src, enc, sizeof(enc)));
  ASSERT_TRUE(toml_decode_string(enc, dec, sizeof(dec)));
  ASSERT_EQ_STR(dec, src);
  ASSERT_FALSE(toml_encode_string("x", enc, 2));
  ASSERT_FALSE(toml_encode_string(NULL, NULL, sizeof(enc)));
}

TEST(config_wrappers_match_common_helpers) {
  char a[64], b[64];
  ASSERT_TRUE(config_toml_encode_string("hello", a, sizeof(a)));
  ASSERT_TRUE(toml_encode_string("hello", b, sizeof(b)));
  ASSERT_EQ_STR(a, b);
  ASSERT_TRUE(config_toml_decode_string("\"hello\"", a, sizeof(a)));
  ASSERT_TRUE(toml_decode_string("\"hello\"", b, sizeof(b)));
  ASSERT_EQ_STR(a, b);
}

TEST(color_ref_parses_hex_forms) {
  ColorRef c = 0;
  ASSERT_TRUE(toml_color_ref("#1a2b3c", &c));
  ASSERT_EQ_INT((int)c, (int)RGB(0x1a, 0x2b, 0x3c));
  ASSERT_TRUE(toml_color_ref("0X1A2B3C", &c));
  ASSERT_EQ_INT((int)c, (int)RGB(0x1a, 0x2b, 0x3c));
  ASSERT_FALSE(toml_color_ref("#12345", &c));
  ASSERT_FALSE(toml_color_ref("#gghhii", &c));
  ASSERT_FALSE(toml_color_ref(NULL, &c));
  ASSERT_FALSE(toml_color_ref("#1a2b3c", NULL));
}

TEST(parse_key_value_shared_parser) {
  char key[384], val[1024];
  const char *line = "  Title = \"hello\" # note";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_OK);
  ASSERT_EQ_STR(key, "Title");
  ASSERT_EQ_STR(val, "\"hello\"");

  line = "\"my key\" = \"v\"";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_OK);
  ASSERT_EQ_STR(key, "my key");
  ASSERT_EQ_STR(val, "\"v\"");

  line = "  # comment only";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_EMPTY);
  line = "no separator";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_NO_KEY);
  line = "= \"x\"";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_INVALID_KEY);
  line = "Key =";
  ASSERT_EQ_INT(
      (int)toml_parse_key_value(&(TomlParseKeyValue){.line = line,
                                                     .len = strlen(line),
                                                     .key = key,
                                                     .key_cap = sizeof(key),
                                                     .value = val,
                                                     .value_cap = sizeof(val)}),
      (int)TOML_KV_INVALID_VALUE);
}

TEST(next_line_handles_crlf_and_final_line) {
  const char *data = "one\r\ntwo\nthree";
  const char *cursor = data, *line = NULL;
  size_t len = 0;
  ASSERT_TRUE(toml_next_line(&cursor, &line, &len));
  ASSERT_EQ_INT((int)len, 3);
  ASSERT_TRUE(memcmp(line, "one", 3) == 0);
  ASSERT_TRUE(toml_next_line(&cursor, &line, &len));
  ASSERT_EQ_INT((int)len, 3);
  ASSERT_TRUE(memcmp(line, "two", 3) == 0);
  ASSERT_TRUE(toml_next_line(&cursor, &line, &len));
  ASSERT_EQ_INT((int)len, 5);
  ASSERT_TRUE(memcmp(line, "three", 5) == 0);
  ASSERT_FALSE(toml_next_line(&cursor, &line, &len));
}

int main(void) {
  RUN(trim_left_and_right);
  RUN(bare_key_validation);
  RUN(strip_comment_keeps_hashes_inside_strings);
  RUN(strip_comment_handles_escaped_quote);
  RUN(quote_balanced_and_control_chars);
  RUN(unquote_strips_quotes_comments_and_space);
  RUN(decode_string_handles_escapes);
  RUN(encode_string_roundtrip);
  RUN(config_wrappers_match_common_helpers);
  RUN(color_ref_parses_hex_forms);
  RUN(parse_key_value_shared_parser);
  RUN(next_line_handles_crlf_and_final_line);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
