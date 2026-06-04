#!/usr/bin/env python3
import os
import sys
import argparse

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildutils import run_cmake_build

script_dir = os.path.dirname(os.path.abspath(__file__))
source_dir = os.path.join(script_dir, "../3rdParty/Sources/imgui")
build_base = os.path.join(script_dir, "../3rdParty/Sources/imgui/_build")
install_base = os.path.join(script_dir, "../3rdParty/imgui")

def build_imgui(target, config_name):
    build_dir = os.path.join(build_base, target, config_name)
    install_dir = os.path.join(install_base, target, config_name)

    # Always use SDL3 as platform backend, regardless of target
    cmake_args = [
        "-DIMGUI_ENABLE_SDL3=ON",
        "-DIMGUI_ENABLE_WIN32=OFF",   # Disable Win32 backend
        "-DIMGUI_ENABLE_DX12=OFF",    # No rendering backend (your engine does rendering)
        "-DIMGUI_BUILD_EXAMPLES=OFF",
        "-DIMGUI_BUILD_DEMO=OFF",
        "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=ON",
    ]

    run_cmake_build(
        source_dir=source_dir,
        build_dir=build_dir,
        install_dir=install_dir,
        config_name=config_name,
        target=target,
        extra_args=cmake_args,
        do_build=True,
        do_install=True,
    )

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo"], default="Release")
    args = parser.parse_args()
    build_imgui(args.target, args.config)
    print(f"\n✅ ImGui built for {args.target} ({args.config})")

if __name__ == "__main__":
    main()