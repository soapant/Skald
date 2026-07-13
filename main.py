import os

text = []

# Target only C source and header files
valid_extensions = (".c", ".h")

folder_contents = os.listdir('.')

for file in folder_contents:
    # 1. Skip folders
    if os.path.isdir(file):
        continue
        
    # 2. Only look at files ending in .c or .h
    if file.endswith(valid_extensions):
        try:
            with open(file, "r", encoding="utf-8") as f:
                content = f.read()
                # Skip files that are actually blank
                if content.strip():
                    text.append(content)
        except (UnicodeDecodeError, PermissionError):
            print(f"Could not read: {file}")

print(text)

