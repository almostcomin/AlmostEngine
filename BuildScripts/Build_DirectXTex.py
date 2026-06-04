#!/usr/bin/env python3
import os
import sys
import argparse

# Add BuildScripts to path
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))

from buildutils import run_cmake_build

script_dir = os.path.dirname(os.path.abspath(__file__))
source_dir = os.path.join(script_dir, "../3rdParty/Sources/DirectXTex")
build_base = os.path.join(script_dir, "../3rdParty/Sources/DirectXTex/_build")
install_base = os.path.join(script_dir, "../3rdParty/DirectXTex")

# Project-specific CMake arguments (common to all targets)
PROJECT_ARGS = [
    "-DBUILD_TOOLS=OFF",
    "-DBUILD_SHARED_LIBS=OFF",
    "-DBUILD_SAMPLE=OFF",
    "-DBUILD_DX11=OFF",   # we only need DX12 for win target, Vulkan for linux
]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo"], default="Release")
    args = parser.parse_args()

    build_dir = os.path.join(build_base, args.target, args.config)
    install_dir = os.path.join(install_base, args.target, args.config)

    run_cmake_build(
        source_dir=source_dir,
        build_dir=build_dir,
        install_dir=install_dir,
        config_name=args.config,
        target=args.target,
        extra_args=PROJECT_ARGS,
        do_build=True,
        do_install=True,
    )

if __name__ == "__main__":
    main()
