#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# Build script for the Tracy profiler server (GUI tool).
# Uses the existing build_project() helper from BuildScripts.
# The server is built in Release mode (no debugging needed for a tool).
# Build artifacts go to a separate folder, and the final executable
# is installed into ../Tools/tracy_server.
# -----------------------------------------------------------------------------

import sys
import os

# Import the common build helper
sys.path.append(os.path.join(os.path.dirname(__file__), "BuildScripts"))
from buildcommon import build_project

# Keep CPM's source cache under the project's _build dir, absolute and short.
# Why: the default cache lives at `<build_dir>/.cpm-cache/` and on Windows the
# checkout of capstone (specifically tests/MC/PowerPC/*) blows past the 260-char
# MAX_PATH because those test fixture filenames are >100 chars long. Putting the
# cache at `3rdParty/_build/cpm_cache/` keeps the full path at ~231 chars on
# Windows. On Linux this is harmless (no MAX_PATH, but consistency is nice).
_script_dir = os.path.dirname(os.path.abspath(__file__))
_cpm_cache = os.path.abspath(os.path.join(_script_dir, "../3rdParty/_build/cpm_cache"))
os.makedirs(_cpm_cache, exist_ok=True)
os.environ["CPM_SOURCE_CACHE"] = _cpm_cache

# Build the profiler server
build_project(
    name="TracyProfiler",
    source_rel="../3rdParty/Sources/tracy/profiler",   # Only the profiler subdir
    build_base_rel="../3rdParty/_build/tracy_profiler", # Separate from client build
    install_base_rel="../Tools/_tracy_server",          # Where the executable goes
    default_args=[],                                    # No extra CMake flags
    extra_args_func=None,                               # No extra args function
)