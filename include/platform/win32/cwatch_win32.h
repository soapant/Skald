#ifndef CWATCH_WIN32_H
#define CWATCH_WIN32_H

#include "core/types.h"

/* Event driven watch over the feature source tree.
   The X11 backend uses inotify for this; on Windows the equivalent is
   ReadDirectoryChangesW, which makes the hot reload watcher cost nothing
   while idle instead of rescanning the whole tree every 500ms.

   win32_cwatch_poll returns a newly allocated path (relative to the watched
   root, using backslashes) of the first changed .c file, or NULL when
   nothing changed. The caller owns the returned string. */

bool  win32_cwatch_active(void);
char* win32_cwatch_poll(const char* root);
void  win32_cwatch_stop(void);

#endif
