# AGENTS.md — Flint Contributor Rules (learned from session)

## Build / Files

- **DO** use `./nob` for Linux and `./nob_linux_window_build` for Windows. `nob.c:74` `obj_path()` uses basename only — two files with same basename (`src/platform/x11/input_x11.c` + `src/platform/x11/input/input_x11.c`) collide to `build/obj/input_x11.c.o` → `multiple definition`. Fix by **moving/renaming** files to unique basenames that keep `_x11.c` suffix so Windows skip (`nob.c:31`) still works, **not** by patching `nob.c` unless explicitly allowed.
- **DO NOT** modify `nob.c` or move files without asking. If a fix requires it, stop and tell the user. Renaming to keep `_x11.c`/`_win32.c` suffix is the minimal move.
- **DO NOT** create new `*.md`/`README` unless asked. `AGENTS.md` is the exception.
- **DO** verify with build: `./nob` + `./nob_linux_window_build` must both `Built Successfully`/`Done: build/flint_win.exe`.

## Platform Split (strict)

- **`fl_` API** (for features) **only** in `include/core/flapi.h` → `src/flapi.c` → `src/platform/x11/flapi_x11.c` / `src/platform/win32/flapi_win32.c` Do Not Include flapi.h or the platform versions in any file that is not a feature file
- **`Platform_` API** (for core) **only** in `include/platform/*.h` → `src/platform/x11/*.c` / `src/platform/win32/*.c` (`Platform_EmitKey`, `Platform_RemapHandleKey`). Core files (`src/core/*`, `src/platform/x11/window_manager_x11.c`) call `Platform_`, never `fl_`.
- **DO NOT** sprinkle `#if defined(_WIN32)` inside functions. Instead have outer shell in `whateverfile.c` that calls `Platform_whatever*`, and provide separate `src/platform/x11/whatever_x11.c` and `src/platform/win32/whatever_win32.c``.Files ending `_x11.c` are auto-skipped on Windows by `nob.c:31` and vice versa .
- **DO** keep platform files minimal and OS-specific: Linux uses `linux/input.h`/`linux/uinput.h`/`evdev`/`uinput`/`XTest` fallback; Windows uses `SendInput`. No cross includes like `<windows.h>` in Linux files.

## Logging

- `include/deps/log.h` `LOG_INFO` gated by `DebugMode` `src/deps/log.c:141` (`if(!DebugMode && level<LOG_WARN) return`). `src/features/*.so` **must** use `printf`, not `log_*`, else `dlopen` undefined `log_log`.
- **DO** use `log_warn`/`log_error` for always-visible debug (no `DebugMode`), or set `DebugMode=true` `include/config.h:25`.
- Include `file_path:line_number` when referencing code.

## UI / Popups / Rendering

- `fl_popup` `src/flapi.c:670` maps `FlPopupSpec.placeholder` / fallback `initial_text→placeholder` (grey), `initial_text` is real prefill. `fl_popup` must be independent of main window: if `wm==NULL` create temp `WindowManager`, else toggle `wm->running` `src/flapi.c:714` so `WindowManager_ShowSimplePrompt` `window_manager.c:422` `while(wm->running && popup->manager)` doesn't exit. On Wayland (`sway`) center via `WindowManager_GetCenteredPos`.

## Rules
- Never Commit Any Changes Or Use Any Git Feature like diff pull push unless user specified (i.e debugging broken code or if the user says to commit after changes and push)
-- when the user says some feature does not work read the files related to that if you can find the issue fix it if you can add log_info in areast where you think the issue  and propmt the user to enable debugging to test the user will give back the entire debug log
-- do not add random comments in the middle of functions under any circumstance

## Communication

- Short, concise, factual, no superlatives/praise/emoji. Use `file:line` refs. Verify via execution, not mental math. Never guess URLs. Respect active corrections until lifted.
- Never Guess Anything about structure of program or program in general if you have something you are not sure of ask the user
