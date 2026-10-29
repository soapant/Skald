# Flint

> A native, lightning-fast control center and launcher for Linux and Windows. 

*(Add a 10-second GIF here showing the launcher opening, typing, and a widget appearing. This is the most important part of the README.)*

Flint is a keyboard-driven launcher and system control center. Unlike other tools that rely on Electron, Node.js, or WebViews, Flint is written entirely in C. It boots instantly, uses virtually no RAM (20-ish Mbs with 22 features will be less with less), and talks directly to your window manager.

## ✨ Why Flint?
- **Modular by Design:** The core is tiny. Everything else (features) are  compiled C modules.
- **Window Manager Native:** Deep integration with X11, Wayland (Sway/Hyprland), and Windows.
- **Zero Dependencies:** (On Windows) statically linked `.exe`. (On Linux) ships with an RPATH bundle. No package manager required to run.

## 🏗️ Build System

Flint uses **Nob** (a single-header build system by Tsoding `https://github.com/tsoding/nob.h`). You only need to compile the build tool once.


**On Linux And Windows:**
```bash
# 1. Compile the build tool
gcc -o nob nob.c

# 2. Run the build
./nob

## 🚀 Quick Start (Linux)

**1. Download and extract**
Download the latest `.tar.gz` from the [Releases page](link) and extract it.
```bash
tar -xzf flint-linux-x86_64.tar.gz
cd flint

**2. Required (Needed For Hotkeys/Remaps To Work Correctly)
sudo usermod -aG input $USER
You must log out and log back in (or reboot) for this group change to take effect. If you skip this, Flint will launch, but your global hotkeys will silently do nothing.

## 🚀 Quick Start (Windows)
Download the latest .exe from the [Releases page](link) and run
./flint
