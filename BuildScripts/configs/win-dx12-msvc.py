# configs/win-dx12-msvc.py
# Native Windows 64-bit build using MSVC (Visual Studio) + DirectX 12

config = {
    "name": "win-dx12-msvc",
    "description": "Windows 64-bit, DirectX 12, MSVC",
    "system": "Windows",
    "arch": "x86_64",
    "compiler": "msvc",

    "cmake_args": [
        "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    ],

    "required_tools": ["cmake"],
}