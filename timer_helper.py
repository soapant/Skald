#!/usr/bin/env python3
"""
Add PROFILE_START/PROFILE_END_VAR instrumentation to C functions.

For each function definition, insert:
    PROFILE_START(_pt);
at the top of the body, and wrap every `return <expr>;` inside the body with:
    { PROFILE_END_VAR(_pt, "<func_name>"); return <expr>; }

Only instruments functions that don't already contain a PROFILE_START call.
"""

import re
import sys
from pathlib import Path


# Matches: <ret_type> <name>(<params>) {
# ret_type can contain spaces, stars, qualifiers, and be spread over lines.
FUNC_RE = re.compile(
    r"""
    ^[ \t]*
    (?P<ret>
        (?:static\s+|inline\s+|extern\s+)*
        [A-Za-z_][A-Za-z0-9_ \t\*]*?
        \s*\*?\s*
    )
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)
    \s*\(
    (?P<params>[^;{}]*?)
    \)
    \s*\{
    """,
    re.MULTILINE | re.VERBOSE | re.DOTALL,
)


def find_functions(src: str):
    """Yield (match_start, name, body_start) for every function definition."""
    for m in FUNC_RE.finditer(src):
        name = m.group("name")
        # Skip function prototypes and anything that looks like a call.
        # A def has `{` immediately after params; the regex guarantees that.
        # Skip if the "return type" is actually a control keyword.
        ret = m.group("ret").strip()
        if re.search(r"\b(if|for|while|switch|return)\b", ret):
            continue
        yield m.start(), name, m.end()  # end == index just after `{`


def find_body_end(src: str, body_start: int) -> int:
    """Return index of the `}` that closes the body starting at body_start-1."""
    depth = 1
    i = body_start
    n = len(src)
    in_str = None
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        if in_line_comment:
            if c == "\n":
                in_line_comment = False
        elif in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 1
        elif in_str:
            if c == "\\":
                i += 1
            elif c == in_str:
                in_str = None
        else:
            if c == "/" and nxt == "/":
                in_line_comment = True
                i += 1
            elif c == "/" and nxt == "*":
                in_block_comment = True
                i += 1
            elif c in "\"'":
                in_str = c
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i
        i += 1

    return n


def find_returns(body: str):
    """Yield (start, end) spans of top-level `return ...;` statements."""
    spans = []
    depth = 0
    i = 0
    n = len(body)
    in_str = None
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = body[i]
        nxt = body[i + 1] if i + 1 < n else ""

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
            depth -= 1
            i += 1
            continue

        # Look for `return` at top level (depth 0 inside the body).
        if depth == 0 and body.startswith("return", i):
            before = body[i - 1] if i > 0 else " "
            after = body[i + 6] if i + 6 < n else " "
            if (not (before.isalnum() or before == "_")) and (
                not (after.isalnum() or after == "_")
            ):
                stmt_start = i
                # find the terminating ';'
                j = i + 6
                while j < n and body[j] != ";":
                    j += 1
                if j < n:
                    spans.append((stmt_start, j + 1))
                    i = j + 1
                    continue
        i += 1

    return spans


def instrument_function(src: str, func_start: int, name: str, body_start: int) -> str:
    """Return src with the function at func_start instrumented."""
    body_end = find_body_end(src, body_start)
    body = src[body_start:body_end]

    if "PROFILE_START(_pt)" in body:
        return src  # already instrumented

    returns = find_returns(body)
    if not returns:
        return src  # nothing to wrap; can't instrument without a return

    # Rebuild body from the end so indices stay valid.
    new_body = body
    for start, end in reversed(returns):
        stmt = new_body[start:end].rstrip()
        # stmt ends with ';'
        inner = stmt[len("return"):].lstrip()
        wrapped = (
            f'{{ PROFILE_END_VAR(_pt, "{name}"); return {inner} }}'
        )
        new_body = new_body[:start] + wrapped + new_body[end:]

    # Insert PROFILE_START at the top of the body.
    # Preserve leading whitespace/newline so the diff is minimal.
    leading_ws = re.match(r"[ \t]*\n?", new_body)
    insert_at = leading_ws.end() if leading_ws else 0
    new_body = (
        new_body[:insert_at]
        + "    PROFILE_START(_pt);\n"
        + new_body[insert_at:]
    )

    return src[:body_start] + new_body + src[body_end:]


def process_file(path: Path) -> bool:
    src = path.read_text()
    original = src

    # Instrument functions from last to first so offsets stay valid.
    funcs = list(find_functions(src))
    for func_start, name, body_start in reversed(funcs):
        src = instrument_function(src, func_start, name, body_start)

    if src != original:
        path.write_text(src)
        return True
    return False


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <file.c>", file=sys.stderr)
        sys.exit(1)

    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"not a file: {path}", file=sys.stderr)
        sys.exit(1)

    if process_file(path):
        print(f"instrumented: {path}")
    else:
        print(f"no changes: {path}")


if __name__ == "__main__":
    main()
