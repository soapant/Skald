count = 0
with open("output.txt","r",encoding='utf-8') as file:
    for line in file:
        if ("printf" in line):
            count += 1;



print(count)
