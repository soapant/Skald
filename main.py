import os

lines = 0
for root, _, files in os.walk("."):
    for file in files:
        if file.endswith((".c", ".h")):
            filepath = os.path.join(root, file)
            try:
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    print(filepath)
                    lines += sum(1 for _ in f)
            except Exception:
                pass

print(lines)
