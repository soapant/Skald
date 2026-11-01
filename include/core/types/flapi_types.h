#ifndef CORE_TYPES_FLAPI_TYPES_H
#define CORE_TYPES_FLAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"

/**
 * @brief A monotonic timer handle.
 *
 * Returned by `fl_timer_start`. Pass it back to
 * `fl_timer_end` to measure elapsed time.
 */
typedef PlTimer FlTimer;

/**
 * @brief Current network connectivity state.
 *
 * Returned by `fl_net_status`.
 */

typedef enum {
  /** @brief No active connection. */
  FL_NET_OFFLINE,
  /** @brief Wired Ethernet connection. */
  FL_NET_ETHERNET,
  /** @brief Wi-Fi connection. */
  FL_NET_WIFI,
} FlNetStatus;

/**
 * @brief Parameters for an edit box widget.
 *
 * Used by `fl_place_editbox`.
 *
 * Fields:
 * - `buffer` — Initial text contents (borrowed, not copied).
 * - `x`, `y` — Top-left corner in screen coordinates.
 * - `w`, `h` — Width and height in pixels.
 * - `color`  — Text color. See `FlColor`
 * - `font`   — Font handle, or 0/NULL for the default font.
 */

typedef struct {
  const char *buffer;
  int x;
  int y;
  int w;
  int h;
  unsigned long color;
  FontHandle font;
} FlEditSpec;

/**
 * @brief Context for a single list item, passed to listbox callbacks.
 *
 * Fields:
 * - `idx`      — Index of this item in the list.
 * - `selected` — Whether this item is currently selected.
 * - `x`, `y`, `w`, `h` — Screen-space bounds of the item.
 * - `text`     — Display text for this item (borrowed).
 */

typedef struct {
  int idx;
  bool selected;
  int x;
  int y;
  int w;
  int h;
  const char *text;
} FlListItemCtx;

#endif
