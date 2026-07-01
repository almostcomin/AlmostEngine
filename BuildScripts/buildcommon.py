#!/usr/bin/env python3
"""
Common build utilities for 3rd-party projects.
Provides a generic function to build any CMake-based project.
"""

import os
import sys
import argparse
from buildutils import run_cmake_build

# List of available configurations
VALID_CONFIGS = ["Debug", "Release", "RelWithDebInfo"]
ALL_CONFIGS = VALID_CONFIGS

def build_project(name, source_rel, build_base_rel, install_base_rel, default_args=None, extra_args_func=None, post_install_func=None):
    """
    Generic function to build a CMake project.
    
    Args:
        name: Project name (for help text)
        source_rel: Relative path from this script to source directory
        build_base_rel: Relative path from this script to build base directory
        install_base_rel: Relative path from this script to install base directory
        default_args: List of default CMake arguments
        extra_args_func: Optional callable that takes (target, config) and returns additional args
        post_install_func: Optional callable invoked after each successful install.
            Signature: post_install_func(target, config, source_dir, install_dir, build_dir).
    """
    parser = argparse.ArgumentParser(description=f"Build {name}")
    parser.add_argument("--target", choices=["win-dx12-clang", "win-dx12-msvc", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=VALID_CONFIGS + ["All"], default="Release",
                        help="Build configuration (Debug, Release, RelWithDebInfo, or All)")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.join(script_dir, source_rel)

    # Determine which configs to build
    if args.config == "All":
        configs_to_build = ALL_CONFIGS
    else:
        configs_to_build = [args.config]

    for config_name in configs_to_build:
        print(f"\n{'='*60}")
        print(f"Building {name} for {args.target} ({config_name})")
        print(f"{'='*60}\n")

        build_dir = os.path.join(script_dir, build_base_rel, args.target, config_name)
        install_dir = os.path.join(script_dir, install_base_rel, args.target, config_name)

        extra_args = list(default_args) if default_args else []
        if extra_args_func:
            extra_args.extend(extra_args_func(args.target, config_name))

        run_cmake_build(
            source_dir=source_dir,
            build_dir=build_dir,
            install_dir=install_dir,
            config_name=config_name,
            target=args.target,
            extra_args=extra_args,
            do_build=True,
            do_install=True,
        )

        if post_install_func is not None:
            post_install_func(args.target, config_name, source_dir, install_dir, build_dir)

        print(f"\n✅ {name} built for {args.target} ({config_name})")