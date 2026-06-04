import os
import subprocess
from setupenv import setup_build_environment

def run_cmake_build(source_dir, build_dir, install_dir, config_name, target, extra_args=None,
                    do_build=True, do_install=True):
    """
    Configure, build, and optionally install a CMake project.
    
    Args:
        source_dir: Source directory
        build_dir: Build directory
        install_dir: Installation directory
        config_name: Build configuration (Debug, Release, etc.)
        target: Target name (e.g., "win-dx12-clang")
        extra_args: Additional CMake arguments (list)
        do_build: Whether to run the build step
        do_install: Whether to run the install step
    """
    # Load configuration for the target
    cfg = setup_build_environment(target)
    
    # Merge environment variables
    env = os.environ.copy()
    env.update(cfg.get("env_vars", {}))
    
    # Build CMake configure command
    cmake_cmd = [
        "cmake", "-S", source_dir, "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={config_name}",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}"
    ]
    cmake_cmd.extend(cfg["cmake_args"])
    if extra_args:
        cmake_cmd.extend(extra_args)
    
    print(f"Configuring for {cfg['name']}...")
    subprocess.run(cmake_cmd, env=env, check=True)
    
    if do_build:
        print(f"Building ({config_name})...")
        build_cmd = ["cmake", "--build", build_dir, "--config", config_name]
        subprocess.run(build_cmd, env=env, check=True)
    
    if do_install:
        print(f"Installing to {install_dir}...")
        install_cmd = ["cmake", "--install", build_dir, "--config", config_name]
        subprocess.run(install_cmd, env=env, check=True)