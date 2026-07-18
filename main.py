import os
import re

ROOT_DIR = "."                     # change to your project root
EXTENSIONS = (".c", ".h")

PATTERNS = [
    # UI_GetSelection
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_GETCURSEL\s*,\s*0\s*,\s*0\s*\)',
     r'UI_GetSelection(\1)'),

    # UI_ResetContent
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_RESETCONTENT\s*,\s*0\s*,\s*0\s*\)',
     r'UI_ResetContent(\1)'),

    # UI_AddString (with optional (LPARAM) cast)
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_ADDSTRING\s*,\s*0\s*,\s*(?:\(LPARAM\)\s*)?([^)]+?)\s*\)',
     r'UI_AddString(\1, \2)'),

    # UI_SetSelection
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_SETCURSEL\s*,\s*([^,]+?)\s*,\s*0\s*\)',
     r'UI_SetSelection(\1, \2)'),

    # UI_GetCount
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_GETCOUNT\s*,\s*0\s*,\s*0\s*\)',
     r'UI_GetCount(\1)'),

    # UI_GetText – captures handle, index, and buffer variable
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_GETTEXT\s*,\s*([^,]+?)\s*,\s*(?:\(LPARAM\)\s*)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\)',
     r'UI_GetText(\1, \2, \3, sizeof(\3))'),

    # UI_SetHorizontalExtent
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*LB_SETHORIZONTALEXTENT\s*,\s*([^,]+?)\s*,\s*0\s*\)',
     r'UI_SetHorizontalExtent(\1, \2)'),
]

def replace_in_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    changed = False
    for pattern, repl in PATTERNS:
        new_content, count = re.subn(pattern, repl, content, flags=re.DOTALL)
        if count:
            changed = True
            content = new_content
            print(f"  Replaced {count} in {os.path.basename(filepath)}")
    if changed:
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            f.write(content)
    return changed

def main():
    for root, dirs, files in os.walk(ROOT_DIR):
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        for file in files:
            if file.endswith(EXTENSIONS):
                fullpath = os.path.join(root, file)
                print(f"Processing {fullpath}")
                replace_in_file(fullpath)

if __name__ == "__main__":
    main()
