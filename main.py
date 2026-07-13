import os

text_blocks = []
valid_extensions = (".c", ".h")

# 1. Gather files and sort them alphabetically so they combine in order
folder_contents = sorted(os.listdir('.'))

for file in folder_contents:
    if os.path.isdir(file):
        continue
        
    if file.endswith(valid_extensions):
        try:
            with open(file, "r", encoding="utf-8") as f:
                content = f.read()
                if content.strip():
                    # Format each file content block beautifully
                    header = f"/* === FILE: {file} === */\n"
                    footer = "\n\n"
                    text_blocks.append(header + content + footer)
        except (UnicodeDecodeError, PermissionError):
            print(f"Could not read: {file}")

# 2. Save everything safely to output.txt
output_filename = "output.txt"
print(text_blocks)
with open(output_filename, "w", encoding="utf-8") as f:
    f.writelines(text_blocks)

print(f"Success! Combined {len(text_blocks)} files into '{output_filename}'.")
