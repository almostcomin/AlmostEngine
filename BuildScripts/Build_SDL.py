#!/usr/bin/env python3
import os
import sys
import argparse

# Add BuildScripts to path
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))

from buildutils import run_cmake_build

script_dir = os.path.dirname(os.path.abspath(__file__))
source_dir = os.path.join(script_dir, "../3rdParty/Sources/SDL")
build_base = os.path.join(script_dir, "../3rdParty/Sources/SDL/_build")
install_base = os.path.join(script_dir, "../3rdParty/SDL")

def build_sdl(target, config_name):
    """
    Build SDL3 as a static library for the given target and configuration.
    """
    build_dir = os.path.join(build_base, target, config_name)
    install_dir = os.path.join(install_base, target, config_name)

    # Common CMake arguments for SDL3 (static build, no shared)
    # Note: For Windows, you might need to set additional flags like
    # -DVIDEO_WIN32=ON etc., but the default CMake configuration handles it.
    cmake_args = [
        "-DSDL_STATIC=ON",
        "-DSDL_SHARED=OFF",
        # Enable only the subsystems you need (optional, helps reduce size)
        "-DSDL_VIDEO=ON",
        "-DSDL_INPUT=ON",
        "-DSDL_EVENTS=ON",
        # Disable unused subsystems
        "-DSDL_AUDIO=OFF",
        "-DSDL_JOYSTICK=OFF",
        "-DSDL_HAPTIC=OFF",
        "-DSDL_SENSOR=OFF",
        "-DSDL_POWER=OFF",
        "-DSDL_FILESYSTEM=OFF",
        "-DSDL_THREAD=OFF",   # You can enable if needed
        "-DSDL_TIMER=OFF",
    ]

    # Platform‑specific adjustments
    if target == "win-dx12-clang":
        # For Windows, we might need to force the generator to use Clang
        # (CMake may default to MSVC if present). The pipeline already
        # sets CC/CXX to clang/clang++, so it should work.
        pass
    elif target == "linux-vk-clang":
        # On Linux, we might want to use Wayland or X11.
        # SDL3 will auto‑detect, but you can force:
        # cmake_args.append("-DVIDEO_WAYLAND=ON")
        # cmake_args.append("-DVIDEO_X11=ON")
        pass

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
    parser = argparse.ArgumentParser(description="Build SDL3 static library")
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo"], default="Release")
    args = parser.parse_args()

    build_sdl(args.target, args.config)
    print(f"\n✅ SDL3 built successfully for {args.target} ({args.config})")

if __name__ == "__main__":
    main()