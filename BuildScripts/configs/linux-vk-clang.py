# configs/linux-vk-clang.py
# Native Linux 64-bit build using Clang + Vulkan

config = {
    "name": "linux-vk-clang",
    "description": "Linux 64-bit, Vulkan, Clang (native)",
    "system": "Linux",
    "arch": "x86_64",
    "compiler": "clang",
    
    "env_vars": {
        "CC": "clang",
        "CXX": "clang++",
    },
    
    "cmake_args": [
        "-DBUILD_VULKAN=ON",
        "-DUSE_VULKAN=ON",
        "-DVULKAN_SDK=/usr",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
    ],
    
    "required_tools": ["clang", "cmake"],
}