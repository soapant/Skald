# Contributing

Skald is a native C project. To keep the codebase fast, maintainable, and consistent, all contributions (whether from humans or AI agents) must follow these rules.

## One Important Rule

**No New Dependencies:** Skald is designed to have zero external runtime dependencies. Do not add new third-party libraries, including vendored single-header ones, without explicit permission.

## Architecture Rules

* **Separation of layers:**
  * `skapi.h` is the public API for features.
  * Core files must **not** include `skapi.h`.
  * If core needs something that is also exposed to features, put the implementation in a core function and make the `skapi` function a thin wrapper around it.
  * This is enforced by `./nob --check`.
* **Struct specs over booleans:** Do not add functions with 5+ boolean/integer arguments (e.g., `func(x, y, w, h, true, false, 1)`). Use designated initializer structs instead (e.g., `sk_draw_rect(.w = 10, .filled = true)`).
* **Public vs. internal:** If a function is only used by the core (not by features), keep it out of `skapi.h` unless a wrapper is needed.

See [DEVELOPMENT.md](DEVELOPMENT.md) for how the platform layer is organized.

## Code Style

* `snake_case` for functions and variables, `PascalCase` for types, `sk_` prefix for the public API, `Platform_` prefix for platform-layer functions(WindowManager_ and UI_ are the only other PascalCase allowed).
* 2-space indentation. Run `clang-format` on the code you change.

## Before You Open a PR

1. It builds with `./nob` on the platform you changed, and you say which one in the PR.
2. `./nob --check` passes.
3. You ran it. For core or feature-loader changes, hot reload a few times in a row.
4. **One change per PR.** No unrelated refactors, formatting-only changes, or mass renames. These will be closed.
5. AI-assisted changes are fine if you understand, tested, and can explain every line. Unreviewed bulk AI changes will be closed.

## Pull Requests

Right now, I am a solo maintainer. I review PRs when I can, so there is no review timeline. Because of this, I will only accept certain types of PRs.

**I WILL ACCEPT PRs FOR:**
*   **Known Issues:** Any fixes addressing the bugs listed in the `Known Issues` section of the README.
*   **Future Plans:** Any steps forward toward the items listed in the `Future Plans` section of the README. For issues labeled `needs-design`, please comment on the issue and agree on an approach before writing code.
*   **Core Improvements:** Addressing open Issues or implementing the core features better (e.g., the Control Center, Clipboard History, Settings, etc.).
*   **Platform Fixes:** Any fixes for crashes on specific Linux distros or operating systems. *(Note: Would be appreciated if an Issue is submitted before writing the PR).*

**I WILL REJECT PRs FOR:**
*   **New Features in this repo:** This repo is for the core, so please don't add new features here unless deemed useful for standard users. If you have a useful feature, open a PR to [skald-features](https://github.com/soapant/skald-features) instead and I might take a look. There are no promises: it depends on my time and on whether it fits.
*   **Architecture Violations:** Any violation of the rules stated in the `Architecture Rules` section.

## Be Civil
Be respectful in issues and PRs. Harassment or personal attacks will get you blocked.

## Security
Found a security problem? Please do not open a public issue. Use GitHub's private vulnerability reporting (the **Security** tab of the repo, then **Report a vulnerability**).

## Licensing
By contributing code, you agree that it will be licensed under Skald's MIT License.
