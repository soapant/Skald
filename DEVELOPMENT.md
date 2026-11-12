# Skald Development

**Note:** The `skapi.h` API is currently unstable and changing rapidly. If you write a feature today, be prepared to update it as the API matures.

## Cross-Platform by Default
Every function in `skapi.h` is designed to be cross-platform. It currently works on **Linux** and **Windows**

It is highly recommended that you only use the API for basic system control. This way, you do not need to use `#ifdef` blocks in your feature code to handle different operating systems.

## Where `Platform_*` Code Lives

`skapi_x11.c` and `skapi_win32.c` are **only** for `sk_*_platform` functions — the platform implementations of the public `skapi.h` API General `Platform_*` helpers live in `platform/` — never in `skapi_x11.c`/ `skapi_win32.c`.

Along with that, the platform files must not `#include "core/skapi.h" or "platform/skapi_platform.h"`. They use `platform.h` and the `core/types/*` headers instead. If removing the include breaks the build, treat it as a bug: the platform layer is not supposed to depend on the public feature API.

## How to Learn the API
If you already know how to program in C, the best way to learn is to read the code itself. **Code is documentation.** You can skip the rest of this file and dive straight into the source code of the core features.

If you prefer a structured overview, read on. You can also browse `skapi.h` directly for the full list of available functions.

## The Framework Structure
You control everything in Skald using callbacks, making the UI more like a framework. You start by creating a main UI window using `sk_window()`

After setting up the window, you simply fill out those callbacks.

**UI Contexts**

You don't have to pass a context to drawing functions (like sk_draw_text or sk_draw_rect). Skald handles the context internally.

For example, if you call a draw function inside the render callback of a button, Skald automatically knows to draw everything relative to that button.

Note: The UI is still early in development and some parts may be buggy or unpolished.
