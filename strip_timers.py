#!/usr/bin/env python3
"""Remove PROFILE_* timers and log_* calls from a C source file.

Companion to timer_helper.py, which adds them.

Handles the common shapes these macros appear in:

    PROFILE_START(_pt);
    if (!x) { PROFILE_END_VAR(_pt, "f"); return NULL; }
    { PROFILE_END_VAR(_pt, "cc_entry_new"); return e; }
    log_warn("thing '%s' broke", v);

Strings and comments are left untouched, so a literal "PROFILE_START(" inside
a printf format string survives.
"""

import re
import sys
from pathlib import Path

STRIP = re.compile(r"\b(PROFILE_START|PROFILE_END|PROFILE_END_VAR|PROFILE_END_TIMER)\s*\(")
LOGS = re.compile(r"\b(log_trace|log_debug|log_info|log_warn|log_error|log_fatal)\s*\(")
INCLUDE_RE = re.compile(r'^[ \t]*#include[ \t]+"core/timer\.h"[ \t]*\n?', re.MULTILINE)


def _skip_ws(src, i):
    while i < len(src) and src[i] in " \t\r\n":
        i += 1
    return i


def _match_paren(src, open_idx):
    """Return index just past the ')' matching the '(' at open_idx."""
    depth = 0
    i = open_idx
    in_str = in_line_comment = in_block_comment = False
    while i < len(src):
        c = src[i]
        nxt = src[i + 1] if i + 1 < len(src) else ""
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
                in_str = False
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
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def _in_code(src, pos):
    """True if pos is in code rather than inside a string or comment."""
    in_str = in_line_comment = in_block_comment = False
    i = 0
    while i < pos and i < len(src):
        c = src[i]
        nxt = src[i + 1] if i + 1 < len(src) else ""
        if in_line_comment:
            if c == "\n":
                in_line_comment = False
        elif in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 2
                continue
        elif in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = False
        elif c == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue
        elif c == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue
        elif c in "\"'":
            in_str = c
        i += 1
    return not (in_str or in_line_comment or in_block_comment)


def strip_calls(src, pattern):
    """Delete every `pattern(...)` call plus its trailing semicolon."""
    removed = 0
    while True:
        changed = False
        for m in pattern.finditer(src):
            if not _in_code(src, m.start()):
                continue
            close = _match_paren(src, m.end() - 1)
            if close < 0:
                continue
            after = _skip_ws(src, close)
            end = after + 1 if after < len(src) and src[after] == ";" else close

            start = m.start()
            # If the call is alone on its line, drop the whole line.
            ls = src.rfind("\n", 0, start) + 1
            line_before = src[ls:start]
            le = src.find("\n", end)
            if le < 0:
                le = len(src)
            line_after = src[end:le]
            if line_before.strip() == "" and line_after.strip() == "":
                end = le + 1 if le < len(src) else le
            src = src[:start] + src[end:]
            removed += 1
            changed = True
            break
        if not changed:
            break
    return src, removed


def tidy(src):
    # Collapse blocks left empty by the removals, e.g. "if (x) {  }".
    prev = None
    while prev != src:
        prev = src
        src = re.sub(r"\{[ \t]*\n?[ \t]*\}", "{}", src)
    return src


def main():
    args = [a for a in sys.argv[1:] if a != "--dry-run"]
    dry = "--dry-run" in sys.argv
    if len(args) != 1:
        print(f"usage: {sys.argv[0]} <file.c> [--dry-run]", file=sys.stderr)
        sys.exit(1)

    path = Path(args[0])
    if not path.is_file():
        print(f"not a file: {path}", file=sys.stderr)
        sys.exit(1)

    src = path.read_text()
    out, n_prof = strip_calls(src, STRIP)
    out, n_log = strip_calls(out, LOGS)
    out = tidy(out)

    if "PROFILE_" not in out:
        out, _ = INCLUDE_RE.subn("", out)

    if out == src:
        print(f"no changes: {path}")
        return

    if dry:
        print(out)
    else:
        path.write_text(out)
    print(f"stripped: {path} (profile={n_prof} log={n_log})")


if __name__ == "__main__":
    main()