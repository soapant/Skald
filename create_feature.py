#!/usr/bin/env python3
import sys
import os
import re

def to_camel(name):
    parts = re.split(r'[_\-\s]+', name)
    return ''.join(p.capitalize() for p in parts if p)

def to_title(name):
    parts = re.split(r'[_\-\s]+', name)
    return ' '.join(p.capitalize() for p in parts if p)

def to_func(name):
    return f"Trigger{to_camel(name)}"

TEMPLATE = '''#include "core/flapi.h"

void {prefix}_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {{
}}

bool {prefix}_enter(const char* selection, ListBoxInfo* lb) {{
    return true;
}}

bool {prefix}_ctrl_enter(const char* selection, ListBoxInfo* lb) {{
    return false;
}}

void {prefix}_copy(const char* selection, int true_index, int type) {{
}}

void {prefix}_tab(ListBoxInfo* lb) {{
}}

void {prefix}_render(PromptState *state) {{
}}

void {func}(void) {{
    PROMPT(.title = "{title}",
           .refresh = {prefix}_refresh,
           .enter = {prefix}_enter,
           .ctrl_enter = {prefix}_ctrl_enter,
           .copy = {prefix}_copy,
           .tab = {prefix}_tab,
           .render = {prefix}_render);
}}

FLINT_FEATURE_INIT {{
    fl_register_hotkey("HOTKEY", {func});
    fl_register_feature("{title}", {func});
}}
'''

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <feature_name>")
        print(f"  e.g. {sys.argv[0]} typing_test")
        print(f"  creates src/features/typing_test.c")
        sys.exit(1)
    raw = sys.argv[1].strip()
    if not raw:
        print("empty name")
        sys.exit(1)
    # sanitize to file name
    fname = re.sub(r'[^a-zA-Z0-9_\\-]', '_', raw).lower()
    if not fname.endswith('.c'):
        file_name = fname + '.c'
    else:
        file_name = fname
        fname = fname[:-2]
    out_path = os.path.join("src", "features", file_name)
    if os.path.exists(out_path):
        print(f"exists: {out_path}")
        sys.exit(1)
    func = to_func(fname)
    title = to_title(fname)
    hotkey = title[0].upper() if title else "A"
    prefix = fname.lower()
    content = TEMPLATE.format(func=func, title=title, hotkey=hotkey, prefix=prefix)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w') as f:
        f.write(content)
    print(f"created {out_path}")

if __name__ == "__main__":
    main()
