# Flint

> A native, lightning-fast control center and launcher for Linux and Windows. 

*(Add a 10-second GIF here showing the launcher opening, typing, and a widget appearing. This is the most important part of the README.)*

Flint is a keyboard-driven launcher and system control center. Unlike other tools that rely on Electron, Node.js, or WebViews, Flint is written entirely in C. It boots instantly, uses virtually no RAM (20-ish Mbs with 22 features will be less with less), and talks directly to your window manager.

## Why Flint?
- **Modular by Design:** The core is tiny. Everything else (features) are  compiled C modules.
- **Window Manager Native:** Deep integration with X11, Wayland (Sway/Hyprland), and Windows.
- **Zero Dependencies:** (On Windows) statically linked `.exe`. (On Linux) ships with an RPATH bundle. No package manager required to run.

##  **Early Development Warning:** 
> Flint is evolving rapidly. The public API (`flapi.h`) is currently **unstable** and changes frequently. If you write your own features right now, be prepared to update your code often as the API matures till the stable release after that release Flint will be backwards compatible.

##  Build System

Flint uses **Nob** (a single-header build system by Tsoding `https://github.com/tsoding/nob.h`). You only need to compile the build tool once.


**On Linux And Windows:**
```bash
# 1. Compile the build tool
gcc -o nob nob.c

# 2. Run the build
./nob

## Quick Start (Linux)

**1. Download and extract**
Download the latest `.tar.gz` from the [Releases page](link) and extract it.
```bash
tar -xzf flint-linux-x86_64.tar.gz
cd flint
./flint
```

**2. Required (Needed For Hotkeys/Remaps To Work Correctly)
sudo usermod -aG input $USER
You must log out and log back in (or reboot) for this group change to take effect. If you skip this, Flint will launch, but your global hotkeys will silently do nothing.

## 🚀 Quick Start (Windows)
Download the latest .exe from the [Releases page](link) and run
```
./flint
```

## How it Works

Flint is split into two halves: **The Core** and **Features**.

*   **The Core (Control Center):** The only hardcoded part of Flint. It handles the global hotkeys for Hot Reloading, Aliases, and the feature loader. It acts as the "OS" of the launcher.
*   **Features:** Everything else is a `.c` file compiled into a `.so` (Linux) or `.dll` (Windows). If you want a clipboard history, you drop `clipboard.c` into the `FeaturePath` folder configured in `config.c`. If you don't want it, just delete it.
*   **Manifests:** Every feature has a `.toml` file in its directory. It stores the feature's name, description, what it provides (via the Capability System), and what dependencies it requires to run.

## Known Issues (Early Development)
Flint is in active, early development. The following bugs are known and will be addressed in future updates. Please do not open a new issue for these unless you have additional debugging information.

1. **Font Hardcoding:** The UI is currently tuned for the `0xProto Nerd Font`. Using any other font will cause text alignment and offset issues.
2. **Windows Image Offset:** Images rendered on Windows currently have an offset placement bug (they are sometimes rendered too far up or down).
3. **Wayland Hotkeys:** Global hotkeys may fail to register on certain Wayland compositors (e.g., GNOME) due to security restrictions. Flint is designed primarily for X11 right now. It should work on Wayland, but don't be surprised if it doesn't.
4. **Wayland Mouse Polling:** Mouse polling does not work on Wayland because the protocol does not easily support querying the global mouse position so dont expect widgets on hover on click to work.

