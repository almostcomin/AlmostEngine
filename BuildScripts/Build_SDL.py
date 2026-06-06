#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

def sdl_extra_args(target, config):
    """Return SDL-specific CMake arguments."""
    base = [
        "-DSDL_STATIC=ON",
        "-DSDL_SHARED=OFF",
        "-DSDL_VIDEO=ON",
        "-DSDL_INPUT=ON",
        "-DSDL_EVENTS=ON",
        "-DSDL_AUDIO=OFF",
        "-DSDL_JOYSTICK=OFF",
        "-DSDL_HAPTIC=OFF",
        "-DSDL_SENSOR=OFF",
        "-DSDL_POWER=OFF",
        "-DSDL_FILESYSTEM=OFF",
        "-DSDL_THREAD=OFF",
        "-DSDL_TIMER=OFF",
    ]
    # Platform-specific additions if needed
    if target == "linux-vk-clang":
        # Could add -DVIDEO_WAYLAND=ON etc.
        pass
    return base

build_project(
    name                = "SDL3",
    source_rel          = "../3rdParty/Sources/SDL",
    build_base_rel      = "../3rdParty/_build/SDL",
    install_base_rel    = "../3rdParty/SDL",
    default_args        = [],  # No default args, use extra_args_func
    extra_args_func     = sdl_extra_args,
)