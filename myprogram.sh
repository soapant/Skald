#!/bin/bash
gcc -DCOBJMACROS  main.c -o myprogram.exe -mwindows -lcomctl32 -lgdiplus -lgdi32 -lshlwapi -lshell32 -lwinmm -lole32 -luuid ; start ./myprogram.exe


