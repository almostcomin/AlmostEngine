import subprocess
import os
import sys

build_dir = os.path.join(os.path.dirname(__file__), "Build")

scripts = [
    f for f in os.listdir(build_dir)
    if f.endswith(".py")
]

for script in scripts:
    path = os.path.join(build_dir, script)
    print(f"===================== Running {script} =====================")
    subprocess.run([sys.executable, path], check=True)