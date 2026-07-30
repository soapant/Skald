lines = []
list_of_duplicate_lines = []
duplicates = 0

with open("output.txt","r",encoding='utf-8') as file:
    for line in file:
        if line not in lines:
            lines.append(line)
        for i in range(len(lines)):
            if line == lines[i]:
                if not "{" in line and not "}" in line and not "#" in line and not "/" in line:
                    if "\n" in line and len(line.strip()) < 4:
                        continue
                    duplicates+=1
                    list_of_duplicate_lines.append(line)


with open ("nigga.txt","w",encoding='utf-8') as file:
    for line in list_of_duplicate_lines:
        line = line.strip()
        file.write(line + "\n")
