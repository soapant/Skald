#ifndef WM_UTIL_H
#define WM_UTIL_H

#include <ctype.h>
#include <string.h>

static inline char *wm_trim_focus_title(char *title) {
  if (!title)
    return NULL;
  while (*title &&
         (iscntrl((unsigned char)*title) || isspace((unsigned char)*title)))
    title++;
  char *end = title + strlen(title);
  while (end > title &&
         (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1])))
    *--end = '\0';
  return title;
}

#endif
