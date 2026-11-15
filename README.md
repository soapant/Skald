# Skald

> A native, lightning-fast control center that so happens to have a launcher for Linux and Windows.

<p align="center">
  <img src="images/cc_modus_vivendi.png" width="800"
       alt="Skald's control center with Modus Vivendi">
</p>

Skald is a native system control center.  Skald is written entirely in C. It boots instantly, uses virtually no RAM,and talks directly to your window manager and its not just a Launcher you can do much more.

## Important Notice:
> **Skald is an engine, not a finished setup.** It provides the pieces (global
> hotkeys, key remapping, clipboard access, overlays, widget slots, a feature
> loader), but a fresh install ships with **no widgets and no remaps**. You add
> what you want as features. Out of the box you get the control center:
> launcher, calculator, hotkey and alias registration.
> this is temporary as a result of early development will be improved on in the future.

> **Looking for example features?** See [skald-features](https://github.com/soapant/skald-features),
> the features I use daily in my own workflow. They're a good reference for
> writing your own. Since the API is unstable, they may break between versions.

## Why Skald?

- **Modular by Design:** The core is tiny. Everything else (features) is compiled C modules.
- **Window Manager Native:** Deep integration with X11, Wayland (Sway/Hyprland), and Windows.
- **Zero Dependencies:** No package manager required to run.

> ⚠️ **Early Development Warning:**
> Skald is evolving rapidly. The public API (`skapi.h`) is currently **unstable** and changes frequently. If you write your own features right now, be prepared to update your code often as the API matures. After the stable release, Skald will maintain backwards compatibility.
>
> If you are a C developer and want to help build the GUI or refine the features or just help in general, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Build System

Skald uses **Nob** (a single-header build system by [Tsoding](https://github.com/tsoding/nob.h)). You only need to compile the build tool once.

**On Linux and Windows:**

```bash
# 1. Compile the build tool
gcc -o nob nob.c

# 2. Run the build
./nob

# 3. -h or --help for list of available commands
./nob -h
```

## Quick Start (Linux)

**1. Download and extract**

Download the latest `.tar.gz` from the [Releases page](https://github.com/soapant/skald/releases) and extract it:

```bash
tar -xzf skald-v#.#.#-linux.tar.gz
cd skald-v#.#.#
./skald
```

**2. Grant permissions (crucial for hotkeys/remaps)**

To listen for global hotkeys and handle remappings, your user must be in the `input` group:

```bash
sudo usermod -aG input $USER
```

> ⚠️ **You must log out and log back in (or reboot) for this group change to take effect.** If you skip this, Skald will launch, but your global hotkeys will silently do nothing or work inconsistently.

## Quick Start (Windows)

Download the latest `.zip` from the [Releases page](https://github.com/soapant/skald/releases), extract it, and run `skald.exe`.

## How it Works

Skald is split into two halves: **the Core** and **Features**.

- **The Core (Control Center):** The only hardcoded part of Skald. It handles global hotkeys for Hot Reloading, Aliases, and the feature loader. It acts as the "OS" of the launcher.
- **Features:** Everything else is a `.c` file compiled into a `.so` (Linux) or `.dll` (Windows). If you want a clipboard history, you drop `clipboard_history.c` into the `FeaturePath` folder configured in `config.c`. If you don't want it, just delete it.
- **Manifests:** Every feature has a `.toml` file in its directory. It stores the feature's name, description, what it provides (via the Capability System), and what dependencies it requires to run.

## Known Issues (Early Development)

Skald is in active, early development. The following bugs are known and will be addressed in future updates. Please do not open a new issue for these unless you have additional debugging information.

1. **Font Hardcoding:** The UI is currently tuned for the `0xProto Nerd Font`. Using any other font will cause text alignment and offset issues.
2. **Wayland Hotkeys:** Global hotkeys or remaps may fail to register on certain Wayland compositors (e.g., GNOME) due to security restrictions. Skald is designed primarily for X11 right now. It should work on Wayland, but don't be surprised if it doesn't.

## 🗺️ Future Plans

- **macOS Support:** Port the platform layer to fully support macOS.
- **Polyglot Features:** Allow features to be written in languages other than C (e.g., Lua, C++, or Rust).
- **Settings GUI:** Build a proper graphical settings menu (currently just an empty feature placeholder).
- **Feature Registry:** A centralized website where users can easily find and download community-made features (like a "Skald Extensions" store), alongside a built-in viewer inside as a feature in Skald.
