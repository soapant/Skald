#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "config_defs.h"

static const char *const table_names[] = {
#define X(type, name, def) #name,
    SKALD_CONFIG_VALUES(X)
#undef X
};
#define TABLE_COUNT (sizeof(table_names) / sizeof(table_names[0]))

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long n = ftell(f);
  if (n < 0 || n > 4 * 1024 * 1024) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[got] = '\0';
  return buf;
}

static size_t table_index_of(const char *name) {
  for (size_t i = 0; i < TABLE_COUNT; i++)
    if (strcmp(table_names[i], name) == 0)
      return i;
  return TABLE_COUNT;
}

static char *config_region(char *cfg, const char **out_end) {
  static char region[65536];
  const char *start_mark = "// <skald-config-generated>";
  const char *end_mark = "// </skald-config-generated>";
  char *start = strstr(cfg, start_mark);
  if (!start)
    return NULL;
  start += strlen(start_mark);
  char *nl = strchr(start, '\n');
  if (!nl)
    return NULL;
  start = nl + 1;
  const char *end = strstr(start, end_mark);
  if (!end || end <= start)
    return NULL;
  size_t len = (size_t)(end - start);
  if (len >= sizeof(region))
    return NULL;
  memcpy(region, start, len);
  region[len] = '\0';
  if (out_end)
    *out_end = end;
  return region;
}

TEST(table_has_no_duplicate_names) {
  ASSERT_TRUE(TABLE_COUNT > 30);
  for (size_t i = 0; i < TABLE_COUNT; i++)
    for (size_t j = i + 1; j < TABLE_COUNT; j++)
      ASSERT_TRUE(strcmp(table_names[i], table_names[j]) != 0);
}

TEST(config_file_has_generated_region_markers) {
  char *cfg = read_file("config/config.c");
  ASSERT_TRUE(cfg != NULL);
  if (!cfg)
    return;
  ASSERT_TRUE(strstr(cfg, "// <skald-config-generated>") != NULL);
  ASSERT_TRUE(strstr(cfg, "// </skald-config-generated>") != NULL);
  free(cfg);
}

TEST(config_file_lists_every_table_entry) {
  char *cfg = read_file("config/config.c");
  ASSERT_TRUE(cfg != NULL);
  if (!cfg)
    return;
  const char *end = NULL;
  char *region = config_region(cfg, &end);
  ASSERT_TRUE(region != NULL);
  if (!region) {
    free(cfg);
    return;
  }
  for (size_t i = 0; i < TABLE_COUNT; i++) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "  %s = ", table_names[i]);
    if (!strstr(region, pattern))
      printf("  missing config assignment for %s\n", table_names[i]);
    ASSERT_TRUE(strstr(region, pattern) != NULL);
  }
  free(cfg);
}

TEST(config_file_has_no_unknown_assignments) {
  char *cfg = read_file("config/config.c");
  ASSERT_TRUE(cfg != NULL);
  if (!cfg)
    return;
  const char *end = NULL;
  const char *region = config_region(cfg, &end);
  ASSERT_TRUE(region != NULL);
  if (!region) {
    free(cfg);
    return;
  }
  char *work = strdup(region);
  ASSERT_TRUE(work != NULL);
  if (!work) {
    free(cfg);
    return;
  }
  char *save = NULL;
  for (char *line = strtok_r(work, "\n", &save); line;
       line = strtok_r(NULL, "\n", &save)) {
    const char *eq = strstr(line, " = ");
    if (!eq)
      continue;
    char name[128];
    size_t nlen = (size_t)(eq - line);
    if (nlen == 0 || nlen >= sizeof(name)) {
      ASSERT_TRUE(0);
      continue;
    }
    memcpy(name, line, nlen);
    name[nlen] = '\0';
    const char *p = name;
    while (*p == ' ' || *p == '\t')
      p++;
    ASSERT_TRUE(table_index_of(p) != TABLE_COUNT);
  }
  free(work);
  free(cfg);
}

int main(void) {
  RUN(table_has_no_duplicate_names);
  RUN(config_file_has_generated_region_markers);
  RUN(config_file_lists_every_table_entry);
  RUN(config_file_has_no_unknown_assignments);
  return TEST_SUMMARY();
}
