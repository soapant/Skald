#!/bin/bash
gcc -DCOBJMACROS  main.c -o main.exe -lcomctl32 -lgdiplus -lgdi32 -lshlwapi -lshell32 -lwinmm -lole32 -luuid -lmsimg32 -ldbghelp -pipe -ldwmapi -Wno-multichar; 
./main.exe
