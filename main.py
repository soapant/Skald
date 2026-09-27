import os

lines = 0
file_count = 0
for root, _, files in os.walk("."):
    if "deps" in root:
        continue
    for file in files:
        if file.endswith((".c", ".h")):
            if "nob" in file or "feature" in root:
                continue
            filepath = os.path.join(root, file)
            try:
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    print(filepath)
                    lines += sum(1 for _ in f)
                file_count += 1
            except Exception:
                pass

print(f"Total lines: {lines}")
print(f"Total files: {file_count}")
print(f"Total lines per file: {int(lines / file_count)}")
