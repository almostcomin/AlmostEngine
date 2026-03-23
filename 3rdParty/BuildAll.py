import subprocess
import os
import sys

build_dir = os.path.join(os.path.dirname(__file__), "Build")
scripts = {"Build_DirectX-Headers.py", "Build_DirectXTex.py", "Build_SDL.py", "Build_imgui.py", "Build_stb.py"}

for script in scripts:
    path = os.path.join(build_dir, script)
    print(f"===================== Running {script} =====================")
    subprocess.run([sys.executable, path], check=True)