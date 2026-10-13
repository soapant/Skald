import os

lines = 0
feature_lines = 0

for root, _, files in os.walk("."):
    if "deps" in root:
        continue
    for file in files:
        if file.endswith((".c", ".h")):
            filepath = os.path.join(root, file)
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                if "feature" in root:
                    feature_lines += sum(1 for _ in f)
                    continue
                lines += sum(1 for _ in f)

print(f"Total Core lines: {lines}")
print(f"Total Feature lines: {feature_lines}")
print(f"Total lines: {lines + feature_lines}")
