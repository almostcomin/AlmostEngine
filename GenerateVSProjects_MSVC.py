#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from setupenv import setup_build_environment

def main():

    target = "win-dx12-msvc"

    # Load target configuration (no -G Ninja, no forced compiler)
    config = setup_build_environment(target)

    engine_root = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(engine_root, "_vsproj", target)

    cmake_cmd = [
        "cmake",
        "-S", engine_root,
        "-B", build_dir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo",
        f"-DALMOSTENGINE_TARGET={target}",
    ]

    # Add arguments from configuration, filtering out those incompatible with VS
    for arg in config.get("cmake_args", []):
        # Skip -G, -DCMAKE_C_COMPILER, -DCMAKE_CXX_COMPILER, etc.
        if (arg.startswith("-G") or
            arg.startswith("-DCMAKE_C_COMPILER") or
            arg.startswith("-DCMAKE_CXX_COMPILER")):
            continue
        cmake_cmd.append(arg)

    # Do not pass CC/CXX environment variables (they could confuse MSVC)
    env = os.environ.copy()
    # Ensure CC/CXX are not defined unless the user forced them
    # (normally they are not needed)
    env.pop("CC", None)
    env.pop("CXX", None)

    print("Generating Visual Studio solution with MSVC...")
    subprocess.run(cmake_cmd, env=env, check=True)
    print(f"\n✅ Solution generated at: {build_dir}\\AlmostEngine.sln")

if __name__ == "__main__":
    main()