# AGENTS.md — Flint Contributor Rules

C11 + X11/Wayland/Win32 launcher ("Flint"). Core is platform-agnostic; backends live behind a `Platform_` C ABI; features are hot-loadable shared objects.

---

## 2. Platform Split (strict)

- **`fl_` API** (for features) lives **only** in `include/core/flapi.h` → `src/flapi.c` → `src/platform/x11/flapi_x11.c` / `src/platform/win32/flapi_win32.c`. Do **not** include `flapi.h` or the platform `flapi_*.c` from any file that is not a feature.
- **`Platform_` API** (for core) lives **only** in `include/platform/*.h` → `src/platform/x11/*.c` / `src/platform/win32/*.c`. Core files (`src/core/*`, `src/platform/x11/window_manager_x11.c`) call `Platform_`, never `fl_`.
- **DO NOT** sprinkle `#if defined(_WIN32)` inside function bodies. Put an outer shell in `whateverfile.c` that calls `Platform_whatever*`, and add `src/platform/x11/whatever_x11.c` + `src/platform/win32/whatever_win32.c`. Files ending `_x11.c` are skipped on Windows by `nob.c:43`; `_win32.c` by `nob.c:46`; `wm.c` by `nob.c:44`.
- **DO** keep platform files minimal and OS-specific: Linux uses `linux/input.h` / `linux/uinput.h` / evdev / uinput / `XTest`; Windows uses `SendInput`. No `<windows.h>` in Linux files.

### Known layering violations (do not extend these)
1. `include/core/flapi.h:436,439,441` declares `Platform_PreRender`, `Platform_SetPreRenderState`, `Platform_SetPreRenderStateWin32` — all also in `include/platform/draw.h:48,34,15`.
2. `src/platform/x11/platform_x11.c:47` and `src/platform/win32/platform_win32.c:25` include `../../../nob.h` for `nob_mkdir_if_not_exists`.
3. `Platform_SettingsPopUp` — header `include/platform/platform.h:58` takes `WindowManager*`, but the Win32 definition `wm_platform_win32.c:273` takes none. Links only because `-std=gnu17`.

---

## 3. Build

- **DO** use `./nob` (Linux) and `./nob_linux_window_build` (Linux→Windows cross). `nob.c:91` `obj_path()` uses the **basename only** — two `.c` files sharing a basename collide into one `build/obj/<base>.o` → `multiple definition`. Fix by renaming to a unique basename that keeps the `_x11.c`/`_win32.c` suffix so the `nob.c:43/46` skips still work. **Not** by patching `nob.c` unless explicitly allowed.
- **DO NOT** modify `nob.c`, `nob_linux_window_build.c`, or move files without asking. If a fix requires it, stop and tell the user.
- Only duplicate basename currently in the tree: `src/core/clipboard_history.c` + `src/features/clipboard_history.c`. Harmless **only** because the feature goes to `build/features/clipboard_history.{so,dll}` via `feature_so_path()` (`nob.c:64`) instead of `obj_path()`. Any *new* duplicate basename in `sources` silently breaks the link.
- **DO** verify with both builds. Success strings: `Built Successfully` (`nob.c:800`) and `Done: build/flint_win.exe (copy to a Windows machine and run)` (`nob_linux_window_build.c:341`).
- Feature `.so`/`.dll` rebuild on header changes via depfiles (`nob.c:649` / `nob.c:712`, `-MF build/features/<base>.d`). If a struct shared across the `.so` boundary changes and features act stale, delete `build/features/*.so` and rebuild.
- **Cross-build gotcha:** `nob_linux_window_build.c` uses mtime-only staleness for feature DLLs (`nob_linux_window_build.c:326-327`, no depfiles), unlike `nob.c`. A header-only change will not rebuild features under the cross build.
- **Wayland is a build-time decision:** `nob.c:48` skips `*_wayland.c` and `nob.c:244` adds `-DFLINT_WAYLAND_SESSION` only when `getenv("WAYLAND_DISPLAY")` is set *in the building shell*. Building from an X11-only shell produces a binary with no Wayland clipboard backend. `nob_linux_window_build.c:28-30` skips `_wayland.c` and `wm.c` unconditionally.
- `nob.c:22` (Linux `CC_FLAGS`) omits `-std=gnu17` while `nob.c:19` (Windows) and `nob_linux_window_build.c:11` both set it — Linux relies on the gcc default.
- `nob.c:646-669` and `nob.c:709-733` duplicate the whole feature-build block; `nob.c:667-669` is a dead empty loop.
- Build always kills the running instance first (`stop_running_instance()`, `nob.c:476`: `systemctl --user stop flint.service; pkill -f 'build/[f]lint'`).
- Flags: `./nob -run` (build+launch), `./nob -test` (build+run tests), `./nob -actual` (Windows only → `build/flint.exe -mwindows` instead of `build/debug/flint.exe`).

---

## 4. Logging

- `src/deps/log.c:134` — `log_log` starts with `if (!DebugMode) return;`. This gates **every** level including `log_warn` and `log_fatal`, not just `LOG_INFO`.
- Consequence: with `DebugMode = false` (`config/config.c:27`, declared `include/config.h:22`, default `src/config_defaults.c:12`) **all** `log_*` output is completely silent. For always-visible diagnostics use `fprintf(stderr, ...)` or ask the user to set `DebugMode = true` in `config/config.c`.
- `src/features/*.so` **must** use `printf`, not `log_*` — `log_log` is not exported into the feature's link, so `log_*` in a feature gives an undefined symbol at `dlopen`.
- Levels: `LOG_TRACE DEBUG INFO WARN ERROR FATAL` (`include/deps/log.h:24`).

---

## 5. Cross-Platform Pixel Parity (strict)

`include/core/prompt_geometry.h` is the **single source of truth** for every popup offset. X11 is the reference implementation. Both backends must be expressed in terms of those constants.

- **DO NOT** reintroduce a bare literal for any quantity named in `prompt_geometry.h` on either platform. If a number is missing, add a constant there.
- **DO NOT** change an X11 geometry value to "fix" a divergence — change the Win32 side. X11's current pixels are the target.
- Win32 prompt geometry that must match X11, and how it is achieved:
  - `EDIT` child at `(PROMPT_EDIT_INSET, PROMPT_EDIT_INSET)`, width `layout->w - PROMPT_EDIT_SCROLL_PAD`, height **the font's real `tm.tmHeight`** (not a constant) — this is what makes the caret land on X11's `14 + (ascent+descent-FontSize)/2`. See `common_ui_win32.c` `CreateChildControls`.
  - `EM_SETMARGINS` with `EC_LEFTMARGIN|EC_RIGHTMARGIN` and lParam `MAKELONG(0,0)` on both `EDIT` controls. This removes the control's built-in 1–2 px text margin, which is the reason the old code had `+2` fudges in its caret maths. **If you add an offset here, the caret drifts.**
  - `LISTBOX` child at `x = 0`, `y = PromptListTop()`, `width = layout->w`, `height = layout->h - PromptListTop() - PromptListBottomMargin()`.
  - `LISTBOX` **must** carry `LBS_NOINTEGRALHEIGHT`. Without it Windows rounds `LB_SETITEMHEIGHT` to a multiple of the font line height and vertically centres the item block, so row 0 is not at the list top — this was the main cause of the listbox y offset being wrong.
  - Item text x is `PROMPT_ITEM_TEXT_X` relative to the listbox client, which is now popup x 0.
  - Rounded selection rect insets horizontally by `PROMPT_SEL_X` to reproduce X11's `bx=4`.
  - `win32_apply_rounded_corners` is called for the prompt/edit/settings popups but X11 can only shape **non**-override-redirect windows (`helpers_x11.c` `x11_apply_shape_rounded` is gated `!override_redirect`). Widgets and message popups are override-redirect on X11 and therefore never rounded, while Win32 does round them. Inert while `RoundedCorners = false`.
- `Platform_GetUIScale()` is hardcoded `1.0f` on Win32 (`ui_scale_win32.c`). It is still consumed by `CalculateWindowMetrics` so the sizing logic matches X11, but Windows has no DPI awareness anywhere in the tree — no `LOGPIXELSX`, `MulDiv`, `GetDpiForWindow`, or `SetProcessDpiAwarenessContext`.

---

## 6. Popups / UI

- `fl_popup` is a macro → `fl_popup_impl` (`src/flapi.c:1010`). It maps `FlPopupSpec.placeholder` via `EditPopup_SetPlaceholder` (grey); `initial_text` is the real prefill. Default size `500x70`, `align` hardcoded `"LEFT"`.
- `fl_popup_impl:1017` takes `wm = WindowManager_Get()` and dereferences it at `:1023` with no NULL check, and it runs its own blocking `while (wm->running && popup->manager) Platform_WM_ProcessEvents(wm)` loop (`:1023-1025`). Anything that changes the WM lifecycle must keep that loop able to exit.
- Popup size/centering: prompt popup `900x640 * Platform_GetUIScale()` (`src/core/popup/prompt_popup.c:153`), settings popup `900x700`. Center via `WindowManager_GetCenteredPos` (`src/core/window_manager.c:125`).
- Popup object model: `Popup` + `PopupVTable` (`include/core/window_manager.h:21,36`), `PopupType` enum at `:30`. `WMEditPopupData` (`:63`) is the struct shared across the `.so` boundary.
- Widgets (`FlWidget`, `flapi.h:394-423`) are persistent desktop windows, not prompt UI. `fl_widget_create` `src/flapi.c:1892`, `fl_widget_relayout` `src/flapi.c:1855` stacks them into `FL_WIDGET_SLOT_*` corners using `WidgetGap`. Render callbacks run inside `Platform_BeginMeasure()`/`Platform_EndMeasure()` so widgets auto-size (`src/flapi.c:1732-1748`).
- Widget hot-reload safety: each widget records its owning `.so` path via `Platform_GetModuleOwnerPath`; `fl_widget_purge_owner` (`src/flapi.c:1771`) kills matching widgets, drops frame callbacks, posts destruction on the render thread, then `Platform_RenderThread_Sync()`s. `src/core/feature_loader.c` calls this around every unload. Never free a widget's render callback yourself.
- X11 runs a **dedicated render thread with its own `Display`** (`Platform_RenderThread_*`, `include/platform/render_thread.h`) so widgets never block popup UI. `src/platform/x11/window_manager_x11.c:258-303` polls **two** X connections and drains both.

---

## 7. Known Bugs / Dead Code (do not "fix" silently — ask first)

- `src/flapi.c:1042` — `fl_draw_text_impl` treats `text` as an integer char code when `(uintptr_t)real_text < 256`. Any small/invalid pointer takes the wrong branch. Pass a real string.
- `src/core/window_manager.c:13` — `SavePerFeatureState` is an empty stub, yet still called from `src/core/popup/prompt_popup.c:109`. `ModePersistsThroughFeatures` + `RememberLastIndex` silently do nothing per-feature.
- `src/core/popup/search_popup.c:10` — `SearchPopup_Create` returns NULL (all params `(void)`). The whole X11 `Platform_SettingsPopup*` family is a no-op (`src/platform/x11/settings_popup_x11.c:5-38`).
- `src/features/todo.c:42` — `#define TODO_TAG_FORMAT TODO_TAG_FORMAT` is self-referential.
- `src/features/zoxide.c:35-64` — `TriggerYaziZoxide`/`yazi_refresh`/`yazi_enter`/`yazi_edit` defined, never registered.
- `include/core/types/file.h:41` — `WinPromptState` defined, never used.
- Duplicate declarations across headers (harmless, `include/core/types/misc.h:51-54` vs `include/platform/win32/common_ui_win32.h`; `include/platform/ui.h:51-53` vs `common_ui_win32.h`; `flapi.h:436-441` vs `draw.h`; `drawing_x11.h:24,25,30,31`).
- Remaining Win32↔X11 divergences **not** fixed in the parity pass — ask before touching:
  - `src/platform/win32/popup/popup_edit_win32.c` never renders `WMEditPopupData.placeholder`; X11 does (`edit_popup_x11.c:149-151`).
  - `Platform_EditPopupRender` is a `(void)` stub on Win32 (`edit_popup_win32.c:51`) — Win32 paints from `EditPopupProc`'s `WM_PAINT` instead.
  - POPUP_EDIT caret on Win32 is the OS caret: it uses the system colour and OS width, X11 draws a `COLOR_CURSOR` rect. Parity needs `CreateCaret` suppression + manual paint.
  - `src/platform/x11/platform_x11.c:854` `Platform_GetTextExtents` sets `*outWidth` to the **last** wrapped line's width, not the max — sizes message windows too narrow. X11-side bug.
  - `include/core/types/prompt.h:9` `ListBoxInfo` embeds fixed `10000 x 512` byte arrays; `MAX_VISIBLE_ITEMS` is 10000 but `Platform_PromptUser` caps at 8192 (`wm_platform_win32.c:94`).
- `src/features/testing.c` contains a slur — do not copy that string into any new file.

---

## 8. Rules

- **Never** commit, diff, pull, push, or use any git feature unless the user explicitly says so.
- **DO NOT** run the program or tests unless the user specifies.
- **DO NOT** add random comments inside function bodies, under any circumstance.
- **DO NOT** create new `*.md` / `README` unless asked. `AGENTS.md` is the exception.
- **DO NOT** add a new `fl_` API function for a single feature's one-off need — implement that logic inside the feature file. A new `fl_` function must serve 2+ features (current or planned).
- When the user reports a feature does not work: read the related files, fix it if the cause is findable. If not, add `log_info` at the suspected point and ask the user to enable `DebugMode` — they will paste the full log back.

---

## 9. Communication

- Short, concise, factual. No superlatives, no praise, no emoji.
- Cite `file_path:line_number` for every claim about code.
- Verify by execution, never by mental math.
- Never guess a URL.
- **Never guess anything about the structure of the program.** If you are not sure, ask the user.
- Respect active corrections until they are explicitly lifted.
