# configs/win-dx12-msvc.py
# Native Windows 64-bit build using MSVC (Visual Studio) + DirectX 12

config = {
    "name": "win-dx12-msvc",
    "description": "Windows 64-bit, DirectX 12, MSVC",
    "system": "Windows",
    "arch": "x86_64",
    "compiler": "msvc",

    "cmake_args": [
        "-DBUILD_DX12=ON",
        "-DBUILD_DX11=OFF",
        "-DBUILD_TOOLS=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_SAMPLE=OFF",
        "-DBC_USE_OPENMP=ON",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    ],

    "required_tools": ["cmake"],
}