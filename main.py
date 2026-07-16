import os
for file in os.listdir('.'):
    if not file == ".git" and not file.endswith(".txt") and not file.endswith(".exe"):
        with open(file, "r", encoding="utf-8") as file:
            line_count = sum(1 for _ in file)

print(f"Total lines: {line_count}")
