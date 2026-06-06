#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

build_project(
    name                = "DirectXTex",
    source_rel          = "../3rdParty/Sources/DirectXTex",
    build_base_rel      = "../3rdParty/_build/DirectXTex",
    install_base_rel    = "../3rdParty/DirectXTex",
    default_args        = [
        "-DBUILD_TOOLS=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_SAMPLE=OFF",
        "-DBUILD_DX11=OFF", ],
)