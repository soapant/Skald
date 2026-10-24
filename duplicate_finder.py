from collections import Counter
import concurrent.futures
from pathlib import Path
import re
import sys

# Optimized C function definition regex
FUNC_DEF_PATTERN = re.compile(
    r"^[ \t]*"  # Optional leading whitespace
    r"(?:(?:static|inline|extern|const|volatile)\s+)*"  # Qualifiers
    r"(?:[a-zA-Z_]\w*(?:\s*\*+\s*|\s+))+"  # Return type
    r"([a-zA-Z_]\w*)"  # Function name
    r"\s*\([^;]*?\)\s*\{",  # Parameters and opening brace
    re.MULTILINE,
)

IGNORED_KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "printf",
    "fprintf",
    "memset",
    "memcpy",
}


def process_single_file(file_path):
  """Worker function to process a single file in parallel."""
  try:
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
      content = f.read()
  except Exception:
    return set(), Counter()

  # 1. Extract defined functions in this file
  defined = set()
  for match in FUNC_DEF_PATTERN.finditer(content):
    func_name = match.group(1)
    if func_name not in IGNORED_KEYWORDS:
      defined.add(func_name)

  # 2. Tokenize and count all whole words in this file in one pass
  words = Counter(re.findall(r"\b[a-zA-Z_]\w*\b", content))

  return defined, words


def main():
  if len(sys.argv) < 2:
    print(f"Usage: python {sys.argv[0]} <target_mention_count>")
    sys.exit(1)

  try:
    target_count = int(sys.argv[1])
  except ValueError:
    print("Error: Target mention count must be an integer.")
    sys.exit(1)

  c_files = [p for p in Path(".").rglob("*") if p.suffix in (".c", ".h")]
  if not c_files:
    print("No .c or .h files found in the current directory.")
    sys.exit(0)

  print(
      f"Scanning {len(c_files)} C/header file(s) with parallel processing..."
  )

  all_defined_funcs = set()
  global_word_counts = Counter()

  # Process files in parallel across CPU cores
  with concurrent.futures.ProcessPoolExecutor() as executor:
    results = executor.map(process_single_file, c_files)
    for defined, words in results:
      all_defined_funcs.update(defined)
      global_word_counts.update(words)

  # Filter by target count
  matching_funcs = [
      func
      for func in all_defined_funcs
      if global_word_counts[func] == target_count
  ]

  # Output results
  print(
      f"\nFunctions mentioned exactly {target_count} time(s)"
      f" ({len(matching_funcs)} found):"
  )
  if matching_funcs:
    for func in sorted(matching_funcs):
      print(f"  - {func}")
  else:
    print("  (None found)")


if __name__ == "__main__":
  main()
