#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from setupenv import setup_build_environment

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["win-dx12-clang"], required=True)
    args = parser.parse_args()

    # Cargamos la configuración original (que contiene -G Ninja y CMAKE_C_COMPILER)
    config = setup_build_environment(args.target)

    engine_root = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(engine_root, "_vsproj", args.target)  # directorio separado

    cmake_cmd = [
        "cmake",
        "-S", engine_root,
        "-B", build_dir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-T", "ClangCL",
        "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo",
        f"-DALMOSTENGINE_TARGET={args.target}",
    ]

    # Añadir argumentos de la configuración, pero filtrando los que no sirven para VS
    for arg in config.get("cmake_args", []):
        # Saltamos -G (cualquier generador), -DCMAKE_C_COMPILER, -DCMAKE_CXX_COMPILER
        if arg.startswith("-G") or arg.startswith("-DCMAKE_C_COMPILER") or arg.startswith("-DCMAKE_CXX_COMPILER"):
            continue
        cmake_cmd.append(arg)

    # También evitamos variables de entorno CC/CXX que puedan confundir
    env = os.environ.copy()
    # No añadimos config.get("env_vars", {}) porque incluiría CC=clang

    print("Generando solución de Visual Studio con ClangCL...")
    subprocess.run(cmake_cmd, env=env, check=True)
    print(f"\n✅ Solución generada en: {build_dir}\\AlmostEngine.sln")

if __name__ == "__main__":
    main()