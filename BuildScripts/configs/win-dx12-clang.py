# configs/win-dx12-clang.py
# Native Windows 64-bit build using Clang + DirectX 12
# This configuration is intended to be used on Windows.

config = {
    "name": "win-dx12-clang",
    "description": "Windows 64-bit, DirectX 12, Clang (native)",
    "system": "Windows",
    "arch": "x86_64",
    "compiler": "clang",
    
    # Environment variables (optional)
    "env_vars": {
        "CC": "clang",
        "CXX": "clang++",
        # You may add Windows-specific flags here if needed
    },
    
    # CMake arguments fixed for this target
    "cmake_args": [
        "-G Ninja",
        "-DBUILD_DX12=ON",
        "-DBUILD_DX11=OFF",
        "-DBUILD_TOOLS=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_SAMPLE=OFF",
        "-DBC_USE_OPENMP=ON",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
    ],
    
    # Optional: list of tools that must be present in PATH
    "required_tools": ["clang", "cmake", "ninja"],
}