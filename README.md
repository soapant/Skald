# Skald

> A native, lightning-fast control center and launcher for Linux and Windows. 

*(Insert your 10-second GIF here showing the launcher opening, typing, and a widget appearing. This is the most important part of the README.)*

Skald is a keyboard-driven launcher and system control center. Unlike other tools that rely on Electron, Node.js, or WebViews, Skald is written entirely in C. It boots instantly, uses virtually no RAM (around 20MB with 22 features loaded, and less with fewer features), and talks directly to your window manager.

##  Why Skald?
- **Modular by Design:** The core is tiny. Everything else (features) is compiled C modules.
- **Window Manager Native:** Deep integration with X11, Wayland (Sway/Hyprland), and Windows.
- **Zero Dependencies:** (On Windows) statically linked `.exe`. (On Linux) ships with an RPATH bundle. No package manager required to run.

>  **Early Development Warning:** 
> Skald is evolving rapidly. The public API (`skapi.h`) is currently **unstable** and changes frequently. If you write your own features right now, be prepared to update your code often as the API matures. After the stable release, Skald will maintain backwards compatibility.

##  Build System

Skald uses **Nob** (a single-header build system by [Tsoding](https://github.com/tsoding/nob.h)). You only need to compile the build tool once.

**On Linux and Windows:**

    # 1. Compile the build tool
    gcc -o nob nob.c

    # 2. Run the build
    ./nob

##  Quick Start (Linux)

**1. Download and extract**
Download the latest `.tar.gz` from the [Releases page](link) and extract it.

    tar -xzf skald-linux-x86_64.tar.gz
    cd skald
    ./skald

**2. Grant permissions (Crucial for Hotkeys/Remaps)**
To listen for global hotkeys and handle remappings, your user must be in the `input` group:

    sudo usermod -aG input $USER

> ⚠️ **You must log out and log back in (or reboot) for this group change to take effect.** If you skip this, Skald will launch, but your global hotkeys will silently do nothing.

##  Quick Start (Windows)

Download the latest `.exe` from the [Releases page](link) and run it:

    skald.exe

##  How it Works

Skald is split into two halves: **The Core** and **Features**.

*   **The Core (Control Center):** The only hardcoded part of Skald. It handles global hotkeys for Hot Reloading, Aliases, and the feature loader. It acts as the "OS" of the launcher.
*   **Features:** Everything else is a `.c` file compiled into a `.so` (Linux) or `.dll` (Windows). If you want a clipboard history, you drop `clipboard.c` into the `FeaturePath` folder configured in `config.c`. If you don't want it, just delete it.
*   **Manifests:** Every feature has a `.toml` file in its directory. It stores the feature's name, description, what it provides (via the Capability System), and what dependencies it requires to run.

##  Known Issues (Early Development)

Skald is in active, early development. The following bugs are known and will be addressed in future updates. Please do not open a new issue for these unless you have additional debugging information.

1. **Font Hardcoding:** The UI is currently tuned for the `0xProto Nerd Font`. Using any other font will cause text alignment and offset issues.
2. **Wayland Hotkeys:** Global hotkeys or remaps may fail to register on certain Wayland compositors (e.g., GNOME) due to security restrictions. Skald is designed primarily for X11 right now. It should work on Wayland, but don't be surprised if it doesn't.

##  Future Plans

*   **macOS Support:** Port the platform layer to fully support macOS.
*   **Polyglot Features:** Allow features to be written in languages other than C (e.g.,Lua,C++, or Rust).
*   **Settings GUI:** Build a proper graphical settings menu (currently just an empty feature placeholder).
*   **Feature Registry:** A centralized website where users can easily find and download community-made features (like a "Skald Extensions" store), alongside a feature in Skald.
*   **Full Git Integration**:
