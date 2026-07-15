import os

file_path = "TerminalHistory.txt"

if os.path.exists(file_path):
    # 1. Read raw contents to check for C-style null characters
    with open(file_path, "r", encoding="utf-8", errors="ignore") as file:
        raw_content = file.read()
    
    # 2. REPAIR: If C-code broke it, swap binary nulls back to standard newlines
    if "\x00" in raw_content:
        raw_content = raw_content.replace("\x00", "\n")
        print("Detected and repaired broken C-style null characters!")

    # 3. Split by newlines and clean up whitespaces safely
    # splitlines() automatically handles \r\n and \n boundaries perfectly
    raw_lines = raw_content.splitlines()
    
    # Strip empty rows and whitespace padding
    cleaned_lines = [line.strip() for line in raw_lines if line.strip()]
    
    # 4. Deduplicate while preserving chronological history order
    unique_lines = list(dict.fromkeys(cleaned_lines))
    
    # 5. Write the crisp rows back with explicit, forced newlines
    with open(file_path, "w", encoding="utf-8") as file:
        file.write("\n".join(unique_lines) + "\n")
        
    print(f"Deduplicated! Reduced down to {len(unique_lines)} clean lines.")
else:
    print(f"Error: {file_path} not found.")

