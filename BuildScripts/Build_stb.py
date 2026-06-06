#!/usr/bin/env python3
import os
import shutil
import glob
import argparse

def main():
    parser = argparse.ArgumentParser(description="Copy stb header files (header-only library)")
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True,
                        help="Target (ignored, stb is platform-independent)")
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo", "All"], default="Release",
                        help="Configuration (ignored, as stb is header-only)")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.join(script_dir, "../3rdParty/Sources/stb")
    install_dir = os.path.join(script_dir, "../3rdParty/stb/include/stb")

    os.makedirs(install_dir, exist_ok=True)

    print(f"Copying stb headers (target={args.target}, config={args.config})...")
    for file_path in glob.glob(os.path.join(source_dir, "*.h")):
        print(f"   Copying: {os.path.basename(file_path)}")
        shutil.copy(file_path, install_dir)

    print("\n✅ Finished")

if __name__ == "__main__":
    main()