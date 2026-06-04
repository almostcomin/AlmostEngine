#!/usr/bin/env python3
import os
import sys
import shutil
import importlib.util

def load_config(target_name):
    """
    Load the configuration for a given target from the configs/ directory.
    Returns a dictionary with the configuration.
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "configs", f"{target_name}.py")
    
    if not os.path.isfile(config_path):
        print(f"ERROR: No configuration found for target '{target_name}'")
        print(f"Expected file: {config_path}")
        sys.exit(1)
    
    # Dynamically load the Python module
    spec = importlib.util.spec_from_file_location(target_name, config_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    
    if not hasattr(module, "config"):
        print(f"ERROR: Configuration file {config_path} must define a 'config' dictionary")
        sys.exit(1)
    
    config = module.config
    # Validate minimal required keys
    required_keys = ["name", "system", "arch", "compiler", "cmake_args"]
    for key in required_keys:
        if key not in config:
            print(f"ERROR: Configuration missing required key '{key}'")
            sys.exit(1)
    
    return config

def check_required_tools(config):
    """Check that all required tools are present in PATH."""
    tools = config.get("required_tools", [])
    missing = []
    for tool in tools:
        if not shutil.which(tool):
            missing.append(tool)
    if missing:
        print(f"ERROR: Missing required tools for '{config['name']}':")
        for t in missing:
            print(f"  - {t}")
        print("\nPlease install the missing tools and try again.")
        sys.exit(1)
    print(f"✓ All required tools for {config['name']} are present.")

def setup_build_environment(target_name):
    """Load configuration and verify tools. Returns the config dict."""
    config = load_config(target_name)
    check_required_tools(config)
    return config