#include "test.h"
#include "core/search_index.h"

static int activated_source = -1;

static void search_test_activate(void *ud, int source_id, SearchKind kind,
                                 const char *widget_name) {
  (void)ud;
  (void)kind;
  (void)widget_name;
  activated_source = source_id;
}

TEST(search_index_query_matches_text_and_context) {
  SearchIndex *idx = search_index_create();
  search_index_push_context(idx, 42, "Display");
  search_index_add_text(&(SearchIndexAddText){.idx = idx,
                                              .text = "Window Opacity",
                                              .kind = SEARCH_KIND_SLIDER,
                                              .widget_name = "WindowOpacity",
                                              .value = "82"});
  search_index_pop_context(idx);

  ASSERT_TRUE(search_index_query(idx, "opacity") > 0);
  const SearchHit *hit = search_index_hit_at(idx, 0);
  ASSERT_TRUE(hit != NULL);
  ASSERT_EQ_INT(hit->source_id, 42);
  ASSERT_EQ_STR(hit->context, "Display");
  ASSERT_EQ_STR(hit->widget_name, "WindowOpacity");
  search_index_destroy(idx);
}

TEST(search_index_query_empty_and_no_match) {
  SearchIndex *idx = search_index_create();
  search_index_add_text(&(SearchIndexAddText){.idx = idx,
                                              .text = "Theme",
                                              .kind = SEARCH_KIND_DROPDOWN,
                                              .widget_name = "Theme",
                                              .value = NULL});
  ASSERT_EQ_INT(search_index_query(idx, ""), 1);
  ASSERT_EQ_INT(search_index_query(idx, "zzzzz"), 0);
  search_index_destroy(idx);
}

TEST(search_index_query_ranks_exact_match_first) {
  SearchIndex *idx = search_index_create();
  search_index_push_context(idx, 1, "RememberLastIndex");
  search_index_add_text(
      &(SearchIndexAddText){.idx = idx,
                            .text = "Open the theme menu.",
                            .kind = SEARCH_KIND_CHECKBOX,
                            .widget_name = "RememberLastIndex",
                            .value = NULL});
  search_index_pop_context(idx);
  search_index_push_context(idx, 2, "Theme");
  search_index_add_text(&(SearchIndexAddText){.idx = idx,
                                              .text = "Theme",
                                              .kind = SEARCH_KIND_DROPDOWN,
                                              .widget_name = "Theme",
                                              .value = NULL});
  search_index_add_text(&(SearchIndexAddText){.idx = idx,
                                              .text = "tokyo-night",
                                              .kind = SEARCH_KIND_DROPDOWN,
                                              .widget_name = "Theme",
                                              .value = NULL});
  search_index_pop_context(idx);

  ASSERT_TRUE(search_index_query(idx, "theme") >= 2);
  const SearchHit *hit = search_index_hit_at(idx, 0);
  ASSERT_TRUE(hit != NULL);
  ASSERT_EQ_INT(hit->source_id, 2);
  search_index_destroy(idx);
}

TEST(search_index_activate_uses_hit_source) {
  SearchIndex *idx = search_index_create();
  activated_source = -1;
  search_index_set_activate(idx, search_test_activate, NULL);
  search_index_push_context(idx, 77, "Config");
  search_index_add_text(&(SearchIndexAddText){.idx = idx,
                                              .text = "Debug Mode",
                                              .kind = SEARCH_KIND_CHECKBOX,
                                              .widget_name = "DebugMode",
                                              .value = NULL});
  search_index_pop_context(idx);
  ASSERT_TRUE(search_index_query(idx, "debug") > 0);
  search_index_activate(idx, 0);
  ASSERT_EQ_INT(activated_source, 77);
  search_index_destroy(idx);
}

int main(void) {
  RUN(search_index_query_matches_text_and_context);
  RUN(search_index_query_empty_and_no_match);
  RUN(search_index_query_ranks_exact_match_first);
  RUN(search_index_activate_uses_hit_source);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
