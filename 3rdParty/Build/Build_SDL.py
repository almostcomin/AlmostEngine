import subprocess
import os

script_dir = os.path.dirname(os.path.abspath(__file__))

source_dir =  os.path.join(script_dir, "../Sources/SDL")
build_base =  os.path.join(script_dir, "../Sources/SDL/build")
install_base =  os.path.join(script_dir, "../SDL")

configs = ["Debug", "Release", "RelWithDebInfo"]

for config in configs:
    build_dir = os.path.join(build_base, config)
    install_dir = os.path.join(install_base, config)

    print(f"\n=== Building {config} ===")

    # Project
    subprocess.run([
        "cmake", "-S", source_dir, "-B", build_dir,
        f"-DSDL_STATIC=OM",
        f"-DSDL_SHARED=OFF",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}"
    ], check=True)

    # Build
    subprocess.run([
        "cmake", "--build", build_dir, "--config", config
    ], check=True)

    # Install
    subprocess.run([
        "cmake", "--install", build_dir, "--config", config
    ], check=True)

print("\n✅ Finished")