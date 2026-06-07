#!/usr/bin/env python3
"""
Build all 3rd-party dependencies for AlmostEngine.

Usage:
    python Build3rdParties.py --target <target> [--config <config>]

Targets:
    win-dx12-clang   - Windows 64-bit with DirectX 12 (Clang)
    linux-vk-clang   - Linux 64-bit with Vulkan (Clang)

Configurations:
    Debug, Release, RelWithDebInfo, or All (compiles all three)
"""

import subprocess
import os
import sys
import argparse

# Add BuildScripts to path (if needed)
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))

# Mapping of scripts to their compatibility with each target
COMPATIBILITY = {
    "Build_DirectX-Headers.py": ["win-dx12-clang", "win-dx12-msvc"],
    "Build_DirectXTex.py":      ["win-dx12-clang", "win-dx12-msvc"],
    "Build_SDL.py":             "all",
    "Build_imgui.py":           "all",
    "Build_stb.py":             "all",
}

# Explicit build order (respects dependencies)
BUILD_ORDER = [
    "Build_SDL.py",             # Must be first because ImGui needs SDL
    "Build_imgui.py",           # Depends on SDL
    "Build_stb.py",             # Header-only, no deps
    "Build_DirectX-Headers.py", # Only for Windows
    "Build_DirectXTex.py",      # Only for Windows
]

# Directory containing all build scripts (same as this script's directory)
BUILD_SCRIPTS_DIR = os.path.dirname(__file__)

def main():
    parser = argparse.ArgumentParser(description="Build all 3rd-party dependencies")
    parser.add_argument("--target", choices=["win-dx12-clang", "win-dx12-msvc", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo", "All"], default="Release",
                        help="Build configuration (Debug, Release, RelWithDebInfo, or All)")
    args = parser.parse_args()

    print(f"\n{'='*60}")
    print(f"Building all dependencies for target: {args.target}")
    print(f"Configuration(s): {args.config}")
    print(f"{'='*60}\n")

    succeeded = []
    failed = []
    skipped = []

    for script_name in BUILD_ORDER:
        # Check compatibility
        compat = COMPATIBILITY.get(script_name, [])
        if compat != "all" and args.target not in compat:
            print(f"⏭️  Skipping {script_name}: incompatible with {args.target}")
            skipped.append(script_name)
            continue

        script_path = os.path.join(BUILD_SCRIPTS_DIR, script_name)
        if not os.path.isfile(script_path):
            print(f"⚠️  Skipping {script_name}: file not found at {script_path}")
            skipped.append(script_name)
            continue

        print(f"\n{'─'*60}")
        print(f"Running {script_name}")
        print(f"{'─'*60}")

        try:
            subprocess.run([
                sys.executable, script_path,
                "--target", args.target,
                "--config", args.config
            ], check=True)
            print(f"✅ {script_name} completed successfully")
            succeeded.append(script_name)
        except subprocess.CalledProcessError as e:
            print(f"❌ {script_name} failed with error: {e}")
            failed.append(script_name)
            # Stop on first failure (dependencies not met)
            break

    # Summary
    print(f"\n{'='*60}")
    print("BUILD SUMMARY")
    print(f"{'='*60}")
    print(f"Target: {args.target}")
    print(f"Configuration(s): {args.config}")
    print(f"\n✅ Succeeded: {len(succeeded)}")
    for s in succeeded:
        print(f"   - {s}")
    print(f"\n⏭️  Skipped: {len(skipped)}")
    for s in skipped:
        print(f"   - {s}")
    if failed:
        print(f"\n❌ Failed: {len(failed)}")
        for f in failed:
            print(f"   - {f}")
        sys.exit(1)
    else:
        print("\n✅ All compatible dependencies built successfully!")

if __name__ == "__main__":
    main()