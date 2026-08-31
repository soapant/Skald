import os

def modify_and_convert_c_to_h():
    src_dir = "/home/diggy/Programming/Projects/C Code/src/features/"
    include_dir = "/home/diggy/Programming/Projects/C Code/include/features/"
    
    os.makedirs(include_dir, exist_ok=True)
    
    for filename in os.listdir(src_dir):
        if not filename.endswith(".c"):
            continue
        
        file_path = os.path.join(src_dir, filename)
        if not os.path.isfile(file_path):
            continue
        
        with open(file_path, "r") as file:
            lines = file.readlines()
        
        modified_lines = []
        for line in lines:
            if "init(void)" in line and not line.strip().startswith("static inline"):
                indent = len(line) - len(line.lstrip())
                modified_lines.append(" " * indent + "static inline " + line.lstrip())
            else:
                modified_lines.append(line)
        
        base_name = os.path.splitext(filename)[0]
        header_filename = base_name + ".h"
        header_path = os.path.join(include_dir, header_filename) 
        
        guard_macro = base_name.upper() + "_H"
        
        with open(header_path, "w") as file:
            file.write(f"#ifndef {guard_macro}\n")
            file.write(f"#define {guard_macro}\n\n")
            file.writelines(modified_lines)
            file.write(f"\n#endif\n")
        
        print(f"Created: {header_path}")

if __name__ == "__main__":
    modify_and_convert_c_to_h()
