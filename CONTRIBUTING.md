# Contributing 

Skald is a native C project. To keep the codebase fast, maintainable, and consistent, all contributions (whether from humans or AI agents) must follow these strict rules.

## The Golden Rules

1. **No Unrequested Refactors:** Do not rewrite existing functions, reformat entire files, or change the build system (`nob`) unless the specific task explicitly requires it.
2. **No New Dependencies:** Skald is designed to have zero external runtime dependencies. Do not add new third-party libraries without explicit permission.
3. **Follow Existing Patterns:** Match the existing code style (4 spaces, snake_case for variables/functions, PascalCase for types). for platform functions or similar its Platform_ then PascalCase.

## Architecture Rules
* **Separation of Layers:** 
  * `skapi.h` is the public API for features **DO NOT** include skapi.h in any core file if you need a core implementation that also has an skapi function it should be a wrapper to Platform_ function. 
  * `platform.h` is the internal OS layer.
  * **Struct Specs over Booleans:** Do not add functions with 5+ boolean/integer arguments (e.g., `func(x, y, w, h, true, false, 1)`). Use designated initializer structs instead (e.g., `func((MySpec){ .w = 10, .filled = true })`).

* **Public vs. Internal:** If a function is only used by the core (not by features), keep it out of `skapi.h`.

## Workflow
1. **Read the code:** If you are an AI agent, read the surrounding files to understand the context before making changes.
2. **Make atomic changes:** Solve one specific problem. Do not fix a bug and refactor a file in the same commit.
3. **Test:** Ensure the code compiles using `./nob` and if you are on linux with mingw you can use `./nob -win32` to test windows build from linux before submitting.

## Pull Requests

Right now, I am a solo maintainer. Because of this, I will only accept certain types of PRs. If your PR falls outside of this list, please keep it in your own fork.

**I WILL ACCEPT PRs FOR:**
*   **Known Issues:** Any fixes addressing the bugs listed in the `Known Issues` section of the README.
*   **Future Plans:** Any steps forward toward the items listed in the `Future Plans` section of the README.
*   **Core Improvements:** Addressing open Issues or implementing the core features better (e.g., the Control Center, Clipboard History, Settings, etc.).
*   **Platform Fixes:** Any fixes for crashes on specific Linux distros or operating systems. *(Note: Please submit an Issue first before writing the PR).*

**I WILL REJECT PRs FOR:**
*   **New Non-Core Features:** Do not submit new features. Features outside the core must be kept in your own fork until I start working on the server and the full feature system (to make downloading/uninstalling new features easy).
*   **Architecture Violations:** Any violation of the rules stated in the `Architecture Rules` section.
*   **AI Mass Refactoring:** Mass refactoring of functions by AI for no good reason.

## Licensing
By contributing code, you agree that it will be licensed under Skald's MIT License.
