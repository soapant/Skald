#!/usr/bin/env python3
import re
import sys
from pathlib import Path


PROFILE_HEADER = '#include "core/timer.h"'


class Scanner:
    def __init__(self, src):
        self.src = src
        self.n = len(src)

    def iter_top_level_returns(self, start, end):
        i = start
        depth = 0
        in_str = None
        in_line_comment = False
        in_block_comment = False

        while i < end:
            c = self.src[i]
            nxt = self.src[i + 1] if i + 1 < end else ""

            if in_line_comment:
                if c == "\n":
                    in_line_comment = False
                i += 1
                continue
            if in_block_comment:
                if c == "*" and nxt == "/":
                    in_block_comment = False
                    i += 2
                    continue
                i += 1
                continue
            if in_str:
                if c == "\\":
                    i += 2
                    continue
                if c == in_str:
                    in_str = None
                i += 1
                continue

            if c == "/" and nxt == "/":
                in_line_comment = True
                i += 2
                continue
            if c == "/" and nxt == "*":
                in_block_comment = True
                i += 2
                continue
            if c in "\"'":
                in_str = c
                i += 1
                continue

            if c == "{":
                depth += 1
                i += 1
                continue
            if c == "}":
                if depth == 0:
                    return
                depth -= 1
                i += 1
                continue

            if depth == 0 and self.src.startswith("return", i):
                before = self.src[i - 1] if i > start else " "
                after = self.src[i + 6] if i + 6 < end else " "
                if not (before.isalnum() or before == "_") and not (
                    after.isalnum() or after == "_"
                ):
                    j = i + 6
                    while j < end and self.src[j] != ";":
                        cj = self.src[j]
                        nj = self.src[j + 1] if j + 1 < end else ""
                        if cj == "/" and nj == "/":
                            while j < end and self.src[j] != "\n":
                                j += 1
                            continue
                        if cj == "/" and nj == "*":
                            j += 2
                            while j < end and not (
                                self.src[j] == "*" and self.src[j + 1 : j + 2] == "/"
                            ):
                                j += 1
                            j += 2
                            continue
                        if cj in "\"'":
                            q = cj
                            j += 1
                            while j < end and self.src[j] != q:
                                if self.src[j] == "\\":
                                    j += 2
                                else:
                                    j += 1
                            j += 1
                            continue
                        j += 1
                    if j < end:
                        yield (i, j + 1)
                        i = j + 1
                        continue

            i += 1

    def find_matching_brace(self, open_idx):
        depth = 0
        i = open_idx
        in_str = None
        in_line_comment = False
        in_block_comment = False

        while i < self.n:
            c = self.src[i]
            nxt = self.src[i + 1] if i + 1 < self.n else ""

            if in_line_comment:
                if c == "\n":
                    in_line_comment = False
                i += 1
                continue
            if in_block_comment:
                if c == "*" and nxt == "/":
                    in_block_comment = False
                    i += 2
                    continue
                i += 1
                continue
            if in_str:
                if c == "\\":
                    i += 2
                    continue
                if c == in_str:
                    in_str = None
                i += 1
                continue

            if c == "/" and nxt == "/":
                in_line_comment = True
                i += 2
                continue
            if c == "/" and nxt == "*":
                in_block_comment = True
                i += 2
                continue
            if c in "\"'":
                in_str = c
                i += 1
                continue

            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i
            i += 1

        return -1


FUNC_RE = re.compile(
    r"""
    (?P<ret>
        (?:^|\n)[ \t]*                           # start of line + leading ws
        (?:
            (?:static|inline|extern|const|unsigned|signed|struct|enum|union)\s+
            |[A-Za-z_][A-Za-z0-9_]*\s+
            |\*+\s*
            |[A-Za-z_][A-Za-z0-9_]*\s*\*+\s*
        )+
    )
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)
    \s*\((?P<params>[^;{}]*?)\)
    \s*\{
    """,
    re.VERBOSE,
)

SKIP_RET_WORDS = {"if", "for", "while", "switch", "return", "do", "else"}


def find_functions(src):
    for m in FUNC_RE.finditer(src):
        ret = m.group("ret")
        words = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", ret)
        if any(w in SKIP_RET_WORDS for w in words):
            continue
        name = m.group("name")
        body_open = m.end() - 1
        yield m.start(), name, body_open


def ensure_include(src, header=PROFILE_HEADER):
    """Insert `header` after the last top-of-file #include, or at the top."""
    if header in src:
        return src, False

    # Find all #include lines that appear before the first non-include,
    # non-comment, non-blank line.
    lines = src.splitlines(keepends=True)
    insert_line = 0
    seen_include = False

    for i, line in enumerate(lines):
        stripped = line.strip()
        if not stripped:
            if seen_include:
                insert_line = i + 1
            continue
        if stripped.startswith("//") or stripped.startswith("/*"):
            if seen_include:
                insert_line = i + 1
            continue
        if stripped.startswith("#include"):
            seen_include = True
            insert_line = i + 1
            continue
        # First real code or non-include directive: stop
        break

    # If we saw at least one include, insert after the last one.
    # If we saw none, insert at position 0 (or after any leading comment block).
    if seen_include:
        new_lines = lines[:insert_line] + [header + "\n"] + lines[insert_line:]
    else:
        new_lines = [header + "\n"] + lines

    return "".join(new_lines), True


def instrument(src: str):
    scanner = Scanner(src)
    funcs = list(find_functions(src))

    edits = []

    for func_start, name, body_open in reversed(funcs):
        body_close = scanner.find_matching_brace(body_open)
        if body_close < 0:
            continue

        body_inner_start = body_open + 1
        body_inner_end = body_close
        body_inner = src[body_inner_start:body_inner_end]

        if "PROFILE_START(_pt)" in body_inner:
            continue

        returns = list(scanner.iter_top_level_returns(body_inner_start, body_inner_end))

        new_inner = body_inner
        for r_start, r_end in reversed(returns):
            rel_start = r_start - body_inner_start
            rel_end = r_end - body_inner_start
            stmt = new_inner[rel_start:rel_end].rstrip()
            inner_expr = stmt[len("return"):].lstrip()
            if inner_expr:
                wrapped = f'{{ PROFILE_END_VAR(_pt, "{name}"); return {inner_expr} }}'
            else:
                wrapped = f'{{ PROFILE_END_VAR(_pt, "{name}"); return; }}'
            new_inner = new_inner[:rel_start] + wrapped + new_inner[rel_end:]

        # Determine indent for PROFILE_START
        first_line_match = re.search(r"\n([ \t]+)\S", new_inner)
        if first_line_match:
            indent = first_line_match.group(1)
        else:
            indent = "    "

        # Insert PROFILE_START at top of body.
        # Preserve leading whitespace/newline.
        m = re.match(r"([ \t]*\n?[ \t]*)", new_inner)
        leading_end = m.end() if m else 0
        leading = new_inner[:leading_end]
        rest = new_inner[leading_end:]
        new_inner = leading + f"{indent}PROFILE_START(_pt);\n" + rest

        edits.append((body_inner_start, body_inner_end - body_inner_start, new_inner))

    if not edits:
        return src, False

    edits.sort(key=lambda e: e[0])
    out = src
    for offset, length, replacement in reversed(edits):
        out = out[:offset] + replacement + out[offset + length :]

    # Ensure the header include is present
    out, _ = ensure_include(out)

    return out, True


def main():
    args = sys.argv[1:]
    dry_run = False
    if "--dry-run" in args:
        dry_run = True
        args.remove("--dry-run")

    if len(args) != 1:
        print(f"usage: {sys.argv[0]} <file.c> [--dry-run]", file=sys.stderr)
        sys.exit(1)

    path = Path(args[0])
    if not path.is_file():
        print(f"not a file: {path}", file=sys.stderr)
        sys.exit(1)

    src = path.read_text()
    out, changed = instrument(src)

    if not changed:
        print(f"no changes: {path}")
        return

    if dry_run:
        print(out)
    else:
        path.write_text(out)
        print(f"instrumented: {path}")


if __name__ == "__main__":
    main()
