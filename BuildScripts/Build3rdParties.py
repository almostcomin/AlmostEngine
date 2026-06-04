#!/usr/bin/env python3
"""
Build all 3rd-party dependencies for AlmostEngine.

Usage:
    python BuildAll.py --target <target> [--config <config>]

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
import time

# Add BuildScripts to path (to use setupenv if needed)
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))

# Mapping of scripts to their compatibility with each target
# - "all": compatible with all targets
# - ["win-dx12-clang"]: only for Windows DirectX
# - ["linux-vk-clang"]: only for Linux Vulkan
# - []: incompatible with all (skip)
COMPATIBILITY = {
    "Build_DirectX-Headers.py": ["win-dx12-clang"],  # DirectX headers only for Windows
    "Build_DirectXTex.py":      ["win-dx12-clang"],  # DirectX texture lib only for Windows
    "Build_SDL.py":             "all",               # SDL3 works everywhere
    "Build_imgui.py":           "all",               # ImGui works everywhere
    "Build_stb.py":             "all",               # stb header-only works everywhere
}

# Directory where build scripts are located
#BUILD_SCRIPTS_DIR = os.path.join(os.path.dirname(__file__), "Build")
BUILD_SCRIPTS_DIR = os.path.dirname(__file__)

# Available configurations
VALID_CONFIGS = ["Debug", "Release", "RelWithDebInfo"]
ALL_CONFIGS = VALID_CONFIGS  # "All" means all of these

def is_compatible(script_name, target):
    """Check if a script is compatible with the given target."""
    compat = COMPATIBILITY.get(script_name, [])
    
    if compat == "all":
        return True
    if isinstance(compat, list):
        return target in compat
    # Default: assume compatible
    return True

def build_single_config(target, config):
    """Build all compatible scripts for a single configuration."""
    print(f"\n{'='*60}")
    print(f"Building for target: {target} (config: {config})")
    print(f"{'='*60}\n")
    
    succeeded = []
    failed = []
    skipped = []
    
    for script_name in COMPATIBILITY.keys():
        script_path = os.path.join(BUILD_SCRIPTS_DIR, script_name)
        
        if not is_compatible(script_name, target):
            print(f"⏭️  Skipping {script_name}: incompatible with {target}")
            skipped.append(script_name)
            continue        

        if not os.path.isfile(script_path):
            print(f"⚠️  Skipping {script_name}: file not found at {script_path}")
            skipped.append(script_name)
            continue
        
        print(f"\n{'─'*60}")
        print(f"Building {script_name}")
        print(f"{'─'*60}")
        
        try:
            subprocess.run([
                sys.executable, script_path,
                "--target", target,
                "--config", config
            ], check=True)
            print(f"✅ {script_name} completed successfully")
            succeeded.append(script_name)
        except subprocess.CalledProcessError as e:
            print(f"❌ {script_name} failed with error: {e}")
            failed.append((script_name, e))
            # Stop on first failure for this config
            return config, succeeded, failed, skipped, False
    
    return config, succeeded, failed, skipped, True

def build_all_configs(target, config_param):
    """Build for one or all configurations based on the config parameter."""
    start_time = time.time()
    
    # Determine which configs to build
    if config_param == "All":
        configs_to_build = ALL_CONFIGS
        print(f"🚀 Building ALL configurations: {', '.join(ALL_CONFIGS)}")
    else:
        configs_to_build = [config_param]
        print(f"🚀 Building single configuration: {config_param}")
    
    all_results = {}
    global_success = True
    
    for config in configs_to_build:
        config_start = time.time()
        print(f"\n{'█'*60}")
        print(f"█  CONFIGURATION: {config}")
        print(f"{'█'*60}")
        
        result = build_single_config(target, config)
        config_name, succeeded, failed, skipped, success = result
        
        elapsed = time.time() - config_start
        all_results[config_name] = {
            "succeeded": succeeded,
            "failed": failed,
            "skipped": skipped,
            "success": success,
            "elapsed": elapsed
        }
        
        if not success:
            global_success = False
            if config_param != "All":
                break  # Stop if single config failed
            else:
                print(f"\n⚠️  {config} failed, but continuing with other configs...")
    
    # Print final summary
    print(f"\n{'='*60}")
    print("FINAL BUILD SUMMARY")
    print(f"{'='*60}")
    print(f"Target: {target}")
    print(f"Total time: {time.time() - start_time:.2f} seconds\n")
    
    for config_name, result in all_results.items():
        status = "✅ PASSED" if result["success"] else "❌ FAILED"
        print(f"{config_name}: {status} ({result['elapsed']:.2f}s)")
        print(f"   ✅ Succeeded: {len(result['succeeded'])}")
        for s in result['succeeded']:
            print(f"      - {s}")
        if result['skipped']:
            print(f"   ⏭️  Skipped: {len(result['skipped'])}")
            for s in result['skipped']:
                print(f"      - {s}")
        if result['failed']:
            print(f"   ❌ Failed: {len(result['failed'])}")
            for f, _ in result['failed']:
                print(f"      - {f}")
        print()
    
    if global_success:
        print("✅ ALL BUILD CONFIGURATIONS COMPLETED SUCCESSFULLY!")
    else:
        print("❌ SOME BUILD CONFIGURATIONS FAILED. Please check errors above.")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Build all 3rd-party dependencies for AlmostEngine",
        epilog="Example: python BuildAll.py --target linux-vk-clang --config All"
    )
    parser.add_argument("--target", choices=["win-dx12-clang", "linux-vk-clang"], required=True)
    parser.add_argument("--config", choices=VALID_CONFIGS + ["All"], default="Release",
                        help="Build configuration (Debug, Release, RelWithDebInfo, or All)")
    args = parser.parse_args()
    
    build_all_configs(args.target, args.config)

if __name__ == "__main__":
    main()