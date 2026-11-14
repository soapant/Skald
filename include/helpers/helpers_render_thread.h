#include <stdbool.h>

#ifndef HELPERS_RENDER_THREAD_H
#define HELPERS_RENDER_THREAD_H

typedef void (*RenderFrameFn)(void *ud);

bool render_thread_ensure_started(int fps);
void render_thread_stop(void);
bool render_thread_is_running(void);
bool render_thread_add_frame_callback(RenderFrameFn fn, void *ud);
bool render_thread_remove_frame_callback(RenderFrameFn fn, const void *ud);
bool render_thread_post(RenderFrameFn fn, void *ud);
void render_thread_wake(void);
void render_thread_sync(void);
bool render_thread_is_current_thread(void);
void *render_thread_display(void);

void render_thread_main(void);

#endif
