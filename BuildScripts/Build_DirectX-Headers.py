#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

build_project(
    name                = "DirectX-Headers",
    source_rel          = "../3rdParty/Sources/DirectX-Headers",
    build_base_rel      = "../3rdParty/_build/DirectX-Headers",
    install_base_rel    = "../3rdParty/DirectX-Headers",
    default_args        = ["-DBUILD_TESTING=OFF"],
)