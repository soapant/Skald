#ifndef X11_DISPLAY_H
#define X11_DISPLAY_H

#include <X11/Xlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to get the centralized X11 display connection.
// Initializes the display if it's not already open.
Display* get_display(void);

// Initializes the X11 display connection and sets up the error handler.
// Returns true on success, false on failure.
bool init_display(void);

// Cleans up and closes the X11 display connection.
void cleanup_display(void);

// Custom X11 error handler.
int x11_error_handler(Display *d, XErrorEvent *e);

#ifdef __cplusplus
}
#endif

#endif // X11_DISPLAY_H
