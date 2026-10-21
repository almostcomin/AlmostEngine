#!/usr/bin/env python3
import sys
import os
import shutil
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project
from pathlib import Path

def install_ktx_artifacts(target, config, source_dir, install_dir, build_dir):
    # The ktx_read target produces a configuration-specific library.
    source_dir = Path(source_dir)
    install_dir = Path(install_dir)
    build_dir = Path(build_dir)

    lib_src = build_dir / "lib" / config / "ktx_read.lib"

    if not lib_src.is_file():
        raise FileNotFoundError(
            f"KTX library not found: {lib_src}"
        )

    # Copy the library without renaming it.
    lib_dst_dir = install_dir / "lib"
    lib_dst_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(lib_src, lib_dst_dir / lib_src.name)

    # Locate the public headers in the source tree.
    header_dirs = [
        source_dir / "include",
        source_dir / "lib" / "include",
    ]
    header_src_dir = next(
        (
            directory
            for directory in header_dirs
            if (directory / "ktx.h").is_file()
        ),
        None,
    )

    if header_src_dir is None:
        raise FileNotFoundError(
            "Could not locate the public KTX headers "
            f"(ktx.h) under {source_dir}"
        )

    # Copy the public API headers that exist in this version.
    public_headers = (
        "ktx.h",
        "ktxgl.h",
        "ktxvulkan.h",
    )

    include_dst_dir = install_dir / "include"
    include_dst_dir.mkdir(parents=True, exist_ok=True)

    copied = []
    for header_name in public_headers:
        header_src = header_src_dir / header_name
        if header_src.is_file():
            shutil.copy2(header_src, include_dst_dir / header_name)
            copied.append(header_name)

    # ktx.h includes KHR/khr_df.h, which lives with the dfdutils sources.
    khr_header_dirs = [
        source_dir / "external" / "dfdutils" / "KHR",
        source_dir / "include" / "KHR",
        source_dir / "lib" / "include" / "KHR",
    ]
    khr_src_dir = next(
        (
            directory
            for directory in khr_header_dirs
            if (directory / "khr_df.h").is_file()
        ),
        None,
    )

    if khr_src_dir is None:
        raise FileNotFoundError(
            "Could not locate the KTX dependency header "
            f"(KHR/khr_df.h) under {source_dir}"
        )

    khr_dst_dir = include_dst_dir / "KHR"
    khr_dst_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(khr_src_dir / "khr_df.h", khr_dst_dir / "khr_df.h")
    copied.append("KHR/khr_df.h")

    print(f"  [KTX] Installed {lib_src.name} -> {lib_dst_dir}")
    print(f"  [KTX] Installed headers {copied} -> {include_dst_dir}")    


build_project(
    name                = "KTX-Software",
    source_rel          = "../3rdParty/Sources/KTX-Software",
    build_base_rel      = "../3rdParty/_build/KTX-Software",
    install_base_rel    = "../3rdParty/KTX-Software",
    default_args        = [
        "-DBUILD_SHARED_LIBS=OFF",
        "-DLIBKTX_VERSION_FULL=OFF",
        "-DLIBKTX_VERSION_READ_ONLY=ON", ],
    build_target="ktx_read",
    do_install=False,
    post_install_func=install_ktx_artifacts,
)