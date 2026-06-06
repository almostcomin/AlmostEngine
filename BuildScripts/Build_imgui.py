#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

def imgui_extra_args(target, config):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sdl_include = os.path.join(script_dir, f"../3rdParty/SDL/{target}/{config}/include")
    sdl_include = os.path.normpath(sdl_include).replace('\\', '/')
    return [f"-DSDL3_INCLUDE_DIR={sdl_include}"]

build_project(
    name                = "ImGui",
    source_rel          = "../3rdParty/Sources/imgui",
    build_base_rel      = "../3rdParty/_build/imgui",
    install_base_rel    = "../3rdParty/imgui",
    default_args        = [
        "-DIMGUI_ENABLE_SDL3=ON",
        "-DIMGUI_ENABLE_WIN32=OFF",
        "-DIMGUI_ENABLE_DX12=OFF",
        "-DIMGUI_BUILD_EXAMPLES=OFF",
        "-DIMGUI_BUILD_DEMO=OFF",
        "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=ON", ],
    extra_args_func     = imgui_extra_args        
)