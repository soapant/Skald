#ifndef HELPERS_TEXT_H
#define HELPERS_TEXT_H

#include "core/types/file.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/types/keys.h"

typedef int (*TextWrapMeasureFn)(void *ctx, const char *text, int len);

typedef struct {
  const char *text;
  ColorRef color;
} TextSegment;

typedef struct {
  char text[256];
  unsigned long color;
  int width;
  int lead;
  int breaks;
  int x;
  int line;
} WrappedWord;

const char *text_resolve(const char *text, char tmp[2]);
bool capture_text(const char *text);
int text_segment_width(const void *seg, int font_size);
void wrapped_word_push(WrappedWord **words, int *count, int *cap, const char *s,
                       size_t len, unsigned long color, int lead, int breaks,
                       int font_size);
typedef struct {
  unsigned long color;
  int font_size;
} WrapCtx;

int wrap_measure_thunk(void *ctx, const char *s, int n);

int text_wrap_layout(const char *text, int x0, int y0, int max_width,
                     int line_height, TextWrapMeasureFn measure, void *ctx,
                     Pos *out, int maxout);

typedef struct {
  char **items;
  size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void string_list_free(StringList *list);
bool safe_copy(char *dst, size_t cap, const char *src);
bool safe_cat(char *dst, size_t cap, const char *src);

long file_read_all(const char *path, char *buffer, long buffer_size);
StringList File_ReadLines(const char *path);
void url_build_search_url(const char *engine, const char *query, char *out,
                          size_t cap);

bool file_walk_all_files_from_dir(const char *dir_path,
                                  char **list_of_allowed_extensions,
                                  ListOfFiles *list_of_files,
                                  char **list_of_names_to_ignore,
                                  bool strip_extension, int max_depth);

char *string_cut_at(const char *str, char delim);
char *string_cut_before(const char *str, char delim);
char *string_cut_after(const char *str, char delim);
char *string_between(const char *str, char open, char close);
char *string_inside(const char *str, char delim);
char *string_trim(const char *str);
char *string_to_lower(const char *str);
char *string_to_upper(const char *str);
bool string_starts_with(const char *str, const char *prefix);
bool string_ends_with(const char *str, const char *suffix);
char *string_replace(const char *str, const char *from, const char *to);
char *string_slice(const char *str, int start, int end);

bool hotkey_text_is_printable(const PopupKeyEvent *key);
bool hotkey_text_should_swallow(const PopupKeyEvent *key);
bool string_contains(const char *haystack, const char *needle);
bool string_contains_ci(const char *haystack, const char *needle);
bool string_starts_with_ci(const char *str, const char *prefix);
bool string_equals_ci(const char *a, const char *b);
int utf8_lead_len(unsigned char c);
bool list_append(WindowList *list, const char *item);
bool window_list_allows(int window_count, bool whitelist, char *const *windows,
                        const char *title);
#endif
