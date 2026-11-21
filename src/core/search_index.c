#include "core/search_index.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEARCH_MAX_ENTRIES 2048
#define SEARCH_MAX_HITS 1024
#define SEARCH_MAX_STACK 64

typedef struct {
  int source_id;
  SearchKind kind;
  char text[256];
  char context[128];
  char widget_name[128];
} SearchEntry;

typedef struct {
  int source_id;
  char context[128];
} SearchContext;

struct SearchIndex {
  SearchEntry entries[SEARCH_MAX_ENTRIES];
  int entry_count;
  SearchHit hits[SEARCH_MAX_HITS];
  int hit_count;
  SearchContext stack[SEARCH_MAX_STACK];
  int stack_depth;
  SearchActivateFn activate;
  void *ud;
};

static __thread SearchIndex *g_capture_index = NULL;

static void search_copy(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src ? src : "");
}

SearchIndex *search_index_create(void) {
  SearchIndex *idx = calloc(1, sizeof(*idx));
  return idx;
}

void search_index_destroy(SearchIndex *idx) { free(idx); }

void search_index_clear(SearchIndex *idx) {
  if (!idx)
    return;
  idx->entry_count = 0;
  idx->hit_count = 0;
  idx->stack_depth = 0;
}

SearchIndex *search_index_current(void) { return g_capture_index; }

void search_index_push_context(SearchIndex *idx, int source_id,
                               const char *context) {
  if (!idx)
    idx = g_capture_index;
  if (!idx || idx->stack_depth >= SEARCH_MAX_STACK)
    return;
  SearchContext *c = &idx->stack[idx->stack_depth++];
  c->source_id = source_id;
  search_copy(c->context, sizeof(c->context), context);
}

void search_index_pop_context(SearchIndex *idx) {
  if (!idx)
    idx = g_capture_index;
  if (!idx || idx->stack_depth <= 0)
    return;
  idx->stack_depth--;
}

void search_index_add_text(SearchIndex *idx, const char *text,
                           SearchKind kind, const char *widget_name,
                           const char *value) {
  if (!idx)
    idx = g_capture_index;
  if (!idx || idx->entry_count >= SEARCH_MAX_ENTRIES)
    return;
  if ((!text || !text[0]) && (!value || !value[0]) &&
      (!widget_name || !widget_name[0]))
    return;

  char combined[256];
  if (text && text[0]) {
    if (value && value[0] && strcmp(text, value) != 0)
      snprintf(combined, sizeof(combined), "%s: %s", text, value);
    else
      search_copy(combined, sizeof(combined), text);
  } else if (value && value[0]) {
    search_copy(combined, sizeof(combined), value);
  } else {
    search_copy(combined, sizeof(combined), widget_name);
  }

  int source_id = -1;
  char context[128] = {0};
  if (idx->stack_depth > 0) {
    const SearchContext *c = &idx->stack[idx->stack_depth - 1];
    source_id = c->source_id;
    search_copy(context, sizeof(context), c->context);
  }

  char name[128];
  search_copy(name, sizeof(name), widget_name);
  for (int i = 0; i < idx->entry_count; i++) {
    const SearchEntry *e = &idx->entries[i];
    if (e->source_id == source_id && e->kind == kind &&
        strcmp(e->text, combined) == 0 && strcmp(e->widget_name, name) == 0)
      return;
  }

  SearchEntry *e = &idx->entries[idx->entry_count++];
  e->source_id = source_id;
  e->kind = kind;
  search_copy(e->text, sizeof(e->text), combined);
  search_copy(e->context, sizeof(e->context), context);
  search_copy(e->widget_name, sizeof(e->widget_name), widget_name);
}

void search_index_set_activate(SearchIndex *idx, SearchActivateFn activate,
                               void *ud) {
  if (!idx)
    return;
  idx->activate = activate;
  idx->ud = ud;
}

void search_index_reindex(SearchIndex *idx, SearchRenderFn render, void *ud) {
  if (!idx || !render || g_capture_index)
    return;
  SearchIndex *saved = g_capture_index;
  g_capture_index = idx;
  search_index_clear(idx);
  render(ud);
  g_capture_index = saved;
}

static void normalize_query(const char *filter, char *out, size_t cap) {
  out[0] = '\0';
  if (!filter)
    return;
  while (*filter && isspace((unsigned char)*filter))
    filter++;
  size_t n = strlen(filter);
  while (n > 0 && isspace((unsigned char)filter[n - 1]))
    n--;
  size_t j = 0;
  for (size_t i = 0; i < n && j + 1 < cap; i++)
    out[j++] = (char)tolower((unsigned char)filter[i]);
  out[j] = '\0';
}

static bool search_word_boundary(const char *s, size_t pos) {
  return pos == 0 || !isalnum((unsigned char)s[pos - 1]);
}

static int score_field(const char *field, const char *query, int penalty) {
  if (!field || !field[0] || !query || !query[0])
    return -1;
  char buf[512];
  size_t n = strlen(field);
  if (n >= sizeof(buf))
    n = sizeof(buf) - 1;
  for (size_t i = 0; i < n; i++)
    buf[i] = (char)tolower((unsigned char)field[i]);
  buf[n] = '\0';

  size_t qn = strlen(query);
  const char *hit = strstr(buf, query);
  if (hit) {
    size_t pos = (size_t)(hit - buf);
    int score;
    if (n == qn)
      score = 0;
    else if (pos == 0)
      score = 10 + (int)(n - qn);
    else if (search_word_boundary(buf, pos))
      score = 30 + (int)pos + (int)(n - qn) / 2;
    else
      score = 80 + (int)pos * 2 + (int)(n - qn) / 4;
    return score + penalty;
  }

  size_t qi = 0;
  int gaps = 0;
  for (size_t i = 0; buf[i] && qi < qn; i++) {
    if (buf[i] == query[qi])
      qi++;
    else
      gaps++;
  }
  if (qi == qn)
    return 300 + gaps + (int)n / 4 + penalty;
  return -1;
}

int search_index_query(SearchIndex *idx, const char *filter) {
  if (!idx)
    return 0;
  idx->hit_count = 0;
  char query[128];
  normalize_query(filter, query, sizeof(query));
  bool empty = query[0] == '\0';

  for (int i = 0; i < idx->entry_count; i++) {
    const SearchEntry *e = &idx->entries[i];
    int score;
    if (empty) {
      score = i;
    } else {
      score = score_field(e->text, query, 0);
      int s = score_field(e->context, query, 2);
      if (s >= 0 && (score < 0 || s < score))
        score = s;
      if (e->widget_name[0] && !strchr(e->widget_name, '/') &&
          strncmp(e->widget_name, "settings-search:", 16) != 0) {
        s = score_field(e->widget_name, query, 10);
        if (s >= 0 && (score < 0 || s < score))
          score = s;
      }
    }
    if (score < 0)
      continue;

    int target = -1;
    for (int j = 0; j < idx->hit_count; j++) {
      if (idx->hits[j].source_id == e->source_id) {
        target = j;
        break;
      }
    }
    if (target >= 0 && score >= idx->hits[target].score)
      continue;
    if (target < 0 && idx->hit_count >= SEARCH_MAX_HITS)
      break;

    SearchHit *h =
        target >= 0 ? &idx->hits[target] : &idx->hits[idx->hit_count++];
    h->source_id = e->source_id;
    h->kind = e->kind;
    h->score = score;
    search_copy(h->text, sizeof(h->text), e->text);
    search_copy(h->context, sizeof(h->context), e->context);
    search_copy(h->widget_name, sizeof(h->widget_name), e->widget_name);
  }

  for (int i = 1; i < idx->hit_count; i++) {
    SearchHit cur = idx->hits[i];
    int j = i - 1;
    while (j >= 0 && (idx->hits[j].score > cur.score ||
                      (idx->hits[j].score == cur.score &&
                       idx->hits[j].source_id > cur.source_id))) {
      idx->hits[j + 1] = idx->hits[j];
      j--;
    }
    idx->hits[j + 1] = cur;
  }
  return idx->hit_count;
}

int search_index_hit_count(const SearchIndex *idx) {
  return idx ? idx->hit_count : 0;
}

const SearchHit *search_index_hit_at(const SearchIndex *idx, int i) {
  if (!idx || i < 0 || i >= idx->hit_count)
    return NULL;
  return &idx->hits[i];
}

void search_index_activate(SearchIndex *idx, int hit_i) {
  if (!idx || !idx->activate || hit_i < 0 || hit_i >= idx->hit_count)
    return;
  const SearchHit *h = &idx->hits[hit_i];
  idx->activate(idx->ud, h->source_id, h->kind, h->widget_name);
}
