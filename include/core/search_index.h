#ifndef CORE_SEARCH_INDEX_H
#define CORE_SEARCH_INDEX_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  SEARCH_KIND_TEXT = 0,
  SEARCH_KIND_BUTTON,
  SEARCH_KIND_CHECKBOX,
  SEARCH_KIND_SLIDER,
  SEARCH_KIND_EDITBOX,
  SEARCH_KIND_DROPDOWN,
  SEARCH_KIND_CUSTOM
} SearchKind;

typedef struct {
  int source_id;
  SearchKind kind;
  char text[256];
  char context[128];
  char widget_name[128];
  int score;
} SearchHit;

typedef struct SearchIndex SearchIndex;

typedef void (*SearchRenderFn)(void *ud);
typedef void (*SearchActivateFn)(void *ud, int source_id, SearchKind kind,
                                 const char *widget_name);

SearchIndex *search_index_create(void);
void search_index_destroy(SearchIndex *idx);
void search_index_clear(SearchIndex *idx);
SearchIndex *search_index_current(void);

void search_index_push_context(SearchIndex *idx, int source_id,
                               const char *context);
void search_index_pop_context(SearchIndex *idx);
void search_index_add_text(SearchIndex *idx, const char *text,
                           SearchKind kind, const char *widget_name,
                           const char *value);
void search_index_set_activate(SearchIndex *idx, SearchActivateFn activate,
                               void *ud);
void search_index_reindex(SearchIndex *idx, SearchRenderFn render, void *ud);

int search_index_query(SearchIndex *idx, const char *filter);
int search_index_hit_count(const SearchIndex *idx);
const SearchHit *search_index_hit_at(const SearchIndex *idx, int i);
void search_index_activate(SearchIndex *idx, int hit_i);

#endif
