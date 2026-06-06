#!/usr/bin/env python3
import os
import sys
import argparse

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildutils import run_cmake_build

ENGINE_ROOT = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = ENGINE_ROOT
BUILD_BASE = os.path.join(ENGINE_ROOT, "_build")
INSTALL_BASE = os.path.join(ENGINE_ROOT, "_install")  # optional

def build_engine(target, config_name):
    build_dir = os.path.join(BUILD_BASE, target, config_name)
    install_dir = os.path.join(INSTALL_BASE, target, config_name)
    extra_args = [f"-DALMOSTENGINE_TARGET={target}"]

    run_cmake_build(
        source_dir=SOURCE_DIR,
        build_dir=build_dir,
        install_dir=install_dir,
        config_name=config_name,
        target=target,
        extra_args=extra_args,
        do_build=True,
        do_install=False,
    )

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo", "All"], default="Release")
    args = parser.parse_args()

    if args.config == "All":
        for cfg in ["Debug", "Release", "RelWithDebInfo"]:
            print(f"\n🔨 Building {args.target} - {cfg}")
            build_engine(args.target, cfg)
        print("\n✅ All configurations built")
    else:
        build_engine(args.target, args.config)
        print(f"\n✅ Engine built for {args.target} ({args.config})")

if __name__ == "__main__":
    main()