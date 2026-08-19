import json
import os
import sys

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_FOLDER = "src"
IS_WINDOWS = sys.platform.startswith("win")

def collect_sources():
    sources = []
    for root, _, files in os.walk(os.path.join(PROJECT_DIR, SRC_FOLDER)):
        for name in files:
            if not name.endswith(".c"):
                continue
            if IS_WINDOWS and name.endswith("_x11.c"):
                continue
            if not IS_WINDOWS and name.endswith("_win32.c"):
                continue
            path = os.path.join(root, name)
            sources.append(os.path.relpath(path, PROJECT_DIR).replace(os.sep, "/"))
    return sorted(sources)

def main():
    entries = []
    for src in collect_sources():
        entries.append({
            "directory": PROJECT_DIR,
            "file": src,
            "arguments": ["gcc", "-Wall", "-Wextra", "-I", "include", "-w", "-c", src],
        })
    out = os.path.join(PROJECT_DIR, "compile_commands.json")
    with open(out, "w") as f:
        json.dump(entries, f, indent=2)
    print(f"wrote {len(entries)} entries to {out}")

if __name__ == "__main__":
    main()
