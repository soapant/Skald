# Contributing 

Skald is a native C project. To keep the codebase fast, maintainable, and consistent, all contributions (whether from humans or AI agents) must follow these strict rules.

## One Important Rule

**No New Dependencies:** Skald is designed to have zero external runtime dependencies. Do not add new third-party libraries without explicit permission.

## Architecture Rules
* **Separation of Layers:** `skapi.h` is the public API for features **DO NOT** include skapi.h in any core file if you need a core implementation that also has an skapi function it should be a wrapper to a core function this is inforced via `./nob -check`. 
* **Struct Specs over Booleans:** Do not add functions with 5+ boolean/integer arguments (e.g., `func(x, y, w, h, true, false, 1)`). Use designated initializer structs instead (e.g., `func((MySpec){ .w = 10, .filled = true })`).
* **Public vs. Internal:** If a function is only used by the core (not by features), keep it out of `skapi.h` unless a wrapper is needed.

## Pull Requests
Right now, I am a solo maintainer. Because of this, I will only accept certain types of PRs.

**I WILL ACCEPT PRs FOR:**
*   **Known Issues:** Any fixes addressing the bugs listed in the `Known Issues` section of the README.
*   **Future Plans:** Any steps forward toward the items listed in the `Future Plans` section of the README.
*   **Core Improvements:** Addressing open Issues or implementing the core features better (e.g., the Control Center, Clipboard History, Settings, etc.).
*   **Platform Fixes:** Any fixes for crashes on specific Linux distros or operating systems. *(Note: Would be appreciated if an Issue is submitted before writing the PR).*

**I WILL REJECT PRs FOR:**
*   **New Non-Core Features:** Do not submit new features. Features outside the core must be kept in your own fork until I start working on the server and the full feature system (to make downloading/uninstalling new features easy).
*   **Architecture Violations:** Any violation of the rules stated in the `Architecture Rules` section.

## Licensing
By contributing code, you agree that it will be licensed under Skald's MIT License.
