# Flint Development Guide

**Note:** The `flapi.h` API is currently unstable and changing rapidly. If you write a feature today, be prepared to update it as the API matures.

## Cross-Platform by Default
Every function in `flapi.h` is designed to be cross-platform. It currently works on **Linux** and **Windows**

It is highly recommended that you only use the API for basic system control. This way, you do not need to use `#ifdef` blocks in your feature code to handle different operating systems.

## How to Learn the API
If you already know how to program in C, the best way to learn is to read the code itself. **Code is documentation.** You can skip the rest of this file and dive straight into the source code of the core features.

If you prefer a structured overview, read on. You can also browse `flapi.h` directly for the full list of available functions, though the auto-generated documentation is currently incomplete. 

## The Framework Structure
You control everything in Flint using callbacks, making the UI more like a framework. You start by creating a main UI window using `fl_window()`:

```c
fl_window(
    .title = "Whatever",
    .refresh = whatever_refresh,
    .enter = whatever_enter,
    .ctrl_enter = whatever_ctrl_enter,
    .copy = whatever_copy,
    .tab = whatever_tab,
    .render = whatever_render
);

After setting up the window, you simply fill out those callbacks.
UI Contexts

You don't have to pass a context to drawing functions (like fl_draw_text or fl_draw_rect). Flint handles the context internally.

For example, if you call a draw function inside the render callback of a button, Flint automatically knows to draw everything relative to that button.

    Note: The UI is still early in development and some parts may be buggy or unpolished.

Icons

If you use fl_register_feature or fl_add_to_cc, you must provide an icon.

Currently, there is no fallback icon. If you do not provide an icon path, the feature or control center entry will show up as empty space

