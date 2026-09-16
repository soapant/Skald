import subprocess

result = subprocess.run(["git", "log", "--format=%H"], capture_output=True, text=True)

raw_output = result.stdout.strip()

commit_hashes = raw_output.split("\n")

for i in range(700):
    result = subprocess.run(["git", "show", f"{commit_hashes[i]}"], capture_output=True, text=True)
    raw_output = result.stdout.strip()
    if raw_output:
        print(raw_output)


