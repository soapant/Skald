import os
import re
import sys

ROOT_DIR = "." 
EXTENSIONS = (".c", ".h")

PATTERNS = [
    # EM_GETSEL
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*EM_GETSEL\s*,\s*\(WPARAM\)&([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*\(LPARAM\)&([a-zA-Z_][a-zA-Z0-9_]*)\s*\)',
     r'UI_Edit_GetSel(\1, &\2, &\3)'),

    # EM_SETSEL
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*EM_SETSEL\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)',
     r'UI_Edit_SetSel(\1, \2, \3)'),

    # SetWindowTextA
    (r'SetWindowTextA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([^)]+?)\s*\)',
     r'UI_SetText(\1, \2)'),

    # GetWindowTextA
    (r'GetWindowTextA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*sizeof\(([a-zA-Z_][a-zA-Z0-9_]*)\)\s*\)',
     r'UI_GetText(\1, \2, sizeof(\3))'),

    (r'GetWindowTextA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([0-9]+)\s*\)',
     r'UI_GetText(\1, \2, \3)'),

    # WM_GETTEXT (for edit controls)
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*WM_GETTEXT\s*,\s*([^,]+?)\s*,\s*\(LPARAM\)([^)]+?)\s*\)',
     r'UI_Edit_GetText(\1, \2, \3)'),

    # WM_SETTEXT
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*WM_SETTEXT\s*,\s*0\s*,\s*\(LPARAM\)([^)]+?)\s*\)',
     r'UI_Edit_SetText(\1, \2)'),

    # WM_GETFONT
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*WM_GETFONT\s*,\s*0\s*,\s*0\s*\)',
     r'UI_Edit_GetFont(\1)'),

    # WM_SETFONT
    (r'SendMessageA\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*WM_SETFONT\s*,\s*\(WPARAM\)([^,]+?)\s*,\s*([^)]+?)\s*\)',
     r'UI_Edit_SetFont(\1, \2, \3)'),
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
        return True
    return False

def main():
    for root, dirs, files in os.walk(ROOT_DIR):
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ['build', 'obj']]
        for file in files:
            if file.endswith(EXTENSIONS) and str(file) != "common_ui.h":
                fullpath = os.path.join(root, file)
                print(f"Processing {fullpath}")
                replace_in_file(fullpath)

if __name__ == "__main__":
    main()
