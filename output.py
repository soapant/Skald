import os
import glob

# Get the current directory
base_dir = os.getcwd()

# Files to exclude
exclude_files = {'output.py', 'output.txt', 'main.exe', 'myprogram.exe'}

# Extensions to include
extensions = ('.c', '.h')

# Get all matching files
all_files = []
for ext in extensions:
    all_files.extend(glob.glob(os.path.join(base_dir, '*' + ext)))

# Filter out excluded files
all_files = [f for f in all_files if os.path.basename(f) not in exclude_files]

# Sort alphabetically
all_files.sort()

with open('output.txt', 'w', encoding='utf-8') as out:
    for filepath in all_files:
        filename = os.path.basename(filepath)
        out.write(f'/* === FILE: {filename} === */\n')
        
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
                out.write(content)
                if not content.endswith('\n'):
                    out.write('\n')
        except Exception as e:
            out.write(f'/* ERROR: Could not read file: {e} */\n')
        
        out.write('\n\n')

print(f"Done! Created output.txt with {len(all_files)} files.")
