import subprocess
import sys
import os


for root,_,files in os.walk(sys.argv[1]):
    for file in files:
        if file.endswith(".c"):
            subprocess.run(["./timer_helper.py", os.path.join(root, file)])
