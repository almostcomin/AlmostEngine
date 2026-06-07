#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from setupenv import setup_build_environment

def main():
    
    target = "win-dx12-clang"

    # Load the original configuration (contains -G Ninja and CMAKE_C_COMPILER)
    config = setup_build_environment(target)

    engine_root = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(engine_root, "_vsproj", target)  # separate directory

    cmake_cmd = [
        "cmake",
        "-S", engine_root,
        "-B", build_dir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-T", "ClangCL",
        "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo",
        f"-DALMOSTENGINE_TARGET={target}",
    ]

    # Add arguments from the configuration, filtering out those unsuitable for VS
    for arg in config.get("cmake_args", []):
        # Skip -G (any generator), -DCMAKE_C_COMPILER, -DCMAKE_CXX_COMPILER
        if arg.startswith("-G") or arg.startswith("-DCMAKE_C_COMPILER") or arg.startswith("-DCMAKE_CXX_COMPILER"):
            continue
        cmake_cmd.append(arg)

    # Also avoid CC/CXX environment variables that might interfere
    env = os.environ.copy()
    # Do not add config.get("env_vars", {}) because it would include CC=clang

    print("Generating Visual Studio solution with ClangCL...")
    subprocess.run(cmake_cmd, env=env, check=True)
    print(f"\n✅ Solution generated at: {build_dir}\\AlmostEngine.sln")

if __name__ == "__main__":
    main()