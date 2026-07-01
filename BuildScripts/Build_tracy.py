#!/usr/bin/env python3
import sys
import os
import shutil
from pathlib import Path

sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

def tracy_extra_args(target, config):
    """Add TRACY_NO_SAMPLING only for Release — sampling in background is the
    biggest idle cost and we don't need it in ship-grade captures."""
    extra_args = []
    if config == "Release":
        extra_args.append("-DTRACY_NO_SAMPLING=ON")
    return extra_args

def patch_tracy_common_headers(target, config, source_dir, install_dir, build_dir):
    """Backfill TracyFormat.hpp. Upstream tracy 0.13.4 installs 15 of 16 files
    in common/, but the explicit `common_includes` list omits TracyFormat.hpp.
    client/TracyProfiler.hpp line 23 needs it transitively, so any consumer
    of <tracy/Tracy.hpp> fails to compile with C1083 otherwise. Idempotent."""
    src = Path(source_dir) / "public" / "common" / "TracyFormat.hpp"
    dst = Path(install_dir) / "include" / "tracy" / "common" / "TracyFormat.hpp"
    if not src.is_file():
        print(f"  [tracy patch] WARNING: {src} not found, skipping")
        return
    if dst.is_file():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    print(f"  [tracy patch] Copied TracyFormat.hpp -> {dst.parent}")

build_project(
    name                = "Tracy",
    source_rel          = "../3rdParty/Sources/tracy",
    build_base_rel      = "../3rdParty/_build/tracy",
    install_base_rel    = "../3rdParty/tracy",
    default_args        = ["-DTRACY_ENABLE=ON", "-DTRACY_ON_DEMAND=ON"],
    extra_args_func     = tracy_extra_args,
    post_install_func   = patch_tracy_common_headers,
)