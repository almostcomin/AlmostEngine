import subprocess
import os

script_dir = os.path.dirname(os.path.abspath(__file__))

source_dir = os.path.join(script_dir, "../Sources/DirectXTex")
build_base = os.path.join(script_dir, "../Sources/DirectXTex/_build")
install_base = os.path.join(script_dir, "../DirectXTex")

configs = ["Debug", "Release", "RelWithDebInfo"]

for config in configs:
    build_dir = os.path.join(build_base, config)
    install_dir = os.path.join(install_base, config)

    print(f"\n=== Building {config} ===")

    # Project
    subprocess.run([
        "cmake", "-S", source_dir, "-B", build_dir,
        "-DBUILD_TOOLS=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_SAMPLE=OFF",
        "-DBUILD_DX11=OFF",
        "-DBUILD_DX12=ON",
        "-DBC_USE_OPENMP=ON",
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