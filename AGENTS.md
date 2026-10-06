# AGENTS.md — Flint Contributor Rules

C11 + X11/Wayland/Win32 launcher ("Flint"). Core is platform-agnostic; backends live behind a `Platform_` C ABI; features are hot-loadable shared objects.

---

## 1. Codebase Map

| Path | Contents |
|---|---|
| `src/main.c` | Entry point. Order: `Platform_TryAcquireSingleInstance` → `InitDataFolder/MainThreadContext/FixWorkingDirectory/InitDisplaySafe` → `Config_Load` + `Config_ResolveDefaultApps` → services (`StartUniversalTimer`, `InitializeTrayIcon`, `InitFeatureDataSystem`, `History_Init`, `Platform_InitUI`) → `FeatureLoader_Init` (dlopens every feature) → `WindowManager_Create` → `WindowManager_Run` → teardown. |
| `src/flapi.c` (2009 L) | **All** `fl_*` implementations + the widget registry. The single biggest file; platform files only supply `_platform` shims. |
| `src/core/` | Platform-agnostic: `window_manager.c` (facade, 174 L), `common_ui.c`, `config_loader.c` (compiles `config/config.c` to a `.dll`/`.so` at runtime and `dlopen`s it), `feature_loader.c`, `history.c`, `remap_manager.c`, `snippet_manager.c`, `hotkey_manager.c`, `vim.c`, `theme.c`, `timer.c`, `prompt_controller.c`, `clipboard_history.c`, `popup/{prompt,edit,settings,search}_popup.c`. |
| `src/platform/x11/` | Linux backend: Xlib + Xft + freetype/fontconfig rendering, evdev/uinput raw key grabbing, `XTest` fallback, inotify watcher, mmap file editing, own-`Display` render thread, raw `wayland-client` clipboard. |
| `src/platform/win32/` | Windows backend: Win32 windows + `commctrl` listboxes + GDI/GDI+ drawing, `SendInput`, `WH_KEYBOARD_LL` hook, tray via `Shell_NotifyIcon`, `LoadLibrary` for features. |
| `src/platform/wm/` | Compositor abstraction. `wm.c` probes i3 → (kwin/niri/hyprland/sway if `WAYLAND_DISPLAY`) → raw X11. Screenshot/OCR is **kwin-only**; every other backend returns `false`. |
| `src/features/` | 26 hot-loadable plugins, one `.c` each, entry via `FLINT_FEATURE_INIT` constructor. |
| `include/core/flapi.h` | The **only** header a feature may include. 210 declared `fl_*` symbols. |
| `include/core/prompt_geometry.h` | **Single source of truth for popup pixel geometry.** Both backends must use these constants. See §5. |
| `include/platform/*.h` | The `Platform_` contract for core files. |
| `include/deps/` | Vendored `log.h`, `stb_ds.h`, `nanosvg.h`, `nanosvgrast.h`. |
| `config/config.c` | The single user-editable config; compiled at runtime. `create_feature.py` scaffolds a new feature. `tests/` are standalone binaries linked against every core `.o` except `main.c`. |

### 30 `fl_` functions are declared in `flapi.h` but have **no definition**
A feature calling one of these gets an undefined symbol at `dlopen` time.

- Raw window access — `flapi.h:217-227`: `fl_get_display`, `fl_get_window`, `fl_get_xftdraw`, `fl_get_visual`, `fl_get_colormap`, `fl_get_hdc`, `fl_get_hwnd`, `fl_get_popup_width`, `fl_get_popup_height`
- Anchored placement — `flapi.h:328-331`: `fl_draw_text_anchored`, `fl_place_editbox_anchored`, `fl_place_listbox_anchored`, `fl_draw_image_anchored`
- Auto layout — `flapi.h:337-346`: `fl_layout_begin/end_{column,row}[_anchored]`, `fl_draw_text_auto`, `fl_place_editbox_auto`, `fl_place_listbox_auto`, `fl_draw_image_auto`
- Images — `flapi.h:448-450`: `fl_load_image`, `fl_free_image`, `fl_draw_image`
- Misc — `flapi.h:112` `fl_init_all_hotkeys`, `flapi.h:121` `fl_launch_browser_in_a_new_window`, `flapi.h:472` `fl_get_text_width`, `flapi.h:474` `fl_is_visible`

Before adding a `fl_` symbol, check it is not already in this list.

---

## 2. Platform Split (strict)

- **`fl_` API** (for features) lives **only** in `include/core/flapi.h` → `src/flapi.c` → `src/platform/x11/flapi_x11.c` / `src/platform/win32/flapi_win32.c`. Do **not** include `flapi.h` or the platform `flapi_*.c` from any file that is not a feature.
- **`Platform_` API** (for core) lives **only** in `include/platform/*.h` → `src/platform/x11/*.c` / `src/platform/win32/*.c`. Core files (`src/core/*`, `src/platform/x11/window_manager_x11.c`) call `Platform_`, never `fl_`.
- **DO NOT** sprinkle `#if defined(_WIN32)` inside function bodies. Put an outer shell in `whateverfile.c` that calls `Platform_whatever*`, and add `src/platform/x11/whatever_x11.c` + `src/platform/win32/whatever_win32.c`. Files ending `_x11.c` are skipped on Windows by `nob.c:43`; `_win32.c` by `nob.c:46`; `wm.c` by `nob.c:44`.
- **DO** keep platform files minimal and OS-specific: Linux uses `linux/input.h` / `linux/uinput.h` / evdev / uinput / `XTest`; Windows uses `SendInput`. No `<windows.h>` in Linux files.

### Known layering violations (do not extend these)
1. `include/core/flapi.h:436,439,441` declares `Platform_PreRender`, `Platform_SetPreRenderState`, `Platform_SetPreRenderStateWin32` — all also in `include/platform/draw.h:48,34,15`.
2. `include/platform/draw.h:49` declares `fl_prerender` (weak-defined in `draw_x11.c` / `draw_win32.c`).
3. `include/core/clipboard_history.h:4` includes `core/flapi.h`, while `src/flapi.c:18` includes `core/clipboard_history.h` — inverted dependency.
4. `src/platform/x11/platform_x11.c:47` and `src/platform/win32/platform_win32.c:25` include `../../../nob.h` for `nob_mkdir_if_not_exists`.
5. `Platform_SettingsPopUp` — header `include/platform/platform.h:58` takes `WindowManager*`, but the Win32 definition `wm_platform_win32.c:273` takes none. Links only because `-std=gnu17`.

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
- `src/features/windows_control.c:32` — `TriggerBlueToothToggle` calls `IsWifiEnabled()`; its log messages are inverted too.
- `src/features/todo.c:42` — `#define TODO_TAG_FORMAT TODO_TAG_FORMAT` is self-referential.
- `src/features/control_center.c:7` — `const char* names[MaxFeatures]` with runtime `MaxFeatures` (1000) → ~8 KB stack VLA.
- `src/features/zoxide.c:35-64` — `TriggerYaziZoxide`/`yazi_refresh`/`yazi_enter`/`yazi_edit` defined, never registered.
- 5 feature files are 100 % commented out and compile to empty `.so`s: `adb.c`, `macros.c`, `mute_microphone.c`, `notification_getter.c`, `window_monitor.c`. 2 more register nothing (`control_center.c:26-30`, `windows_control.c:42-46`). `src/features/reminder.c` is a hollow stub (all callbacks empty).
- `include/core/types/file.h:41` — `WinPromptState` defined, never used.
- `include/core/timer.h:32-37` — `PROFILE_SCOPE` unused and would not compile (calls `_profile_timer_cleanup` at `:35` before declaring it at `:33`).
- `include/config.h:10-11` — `COMMAND_MODE_KEY` / `SEARCH_PROGRAM_KEY` have no users.
- No `TODO:` / `FIXME:` / `XXX:` comments exist in first-party code. `// temporary` markers are at `src/bindings/vim_motions.c:253`, `src/bindings/vim_bindings.c:51`, `src/features/control_center.c:27`, `src/features/windows_control.c:43`, `src/platform/x11/default_apps_x11.c:208`, `src/platform/x11/platform_x11.c:141`, `src/platform/x11/draw_x11.c:525`, `src/platform/win32/popup/popup_main_win32.c:875`.
- Explicit "not implemented yet" stubs: `src/platform/x11/platform_x11.c:617,623,630,643,650`; `src/platform/win32/flapi_win32.c:573,590`.
- Hard-coded personal paths/credentials are committed throughout: `src/features/misc_hotkeys.c:48,58,73,76`, `src/features/app_launcher.c:28-31`, `src/features/file_search.c:11-12`, `src/config_defaults.c:43-48`, `config/config.c:4-11`, `flint.service:10-11`. Do not add more.
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