# AlmostEngine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)]()
[![C++](https://img.shields.io/badge/C++-23-blue.svg)](https://isocpp.org/)

**AlmostEngine** is a personal graphics and game engine project — built to learn, experiment, and showcase modern rendering techniques.

---

## Features

- DirectX 12 Backend
- glTF file loader
- Physically Based Rendering (PBR)
- Fully bindless pipeline
- WBOIT
- Directional, point and spot lights
- Shadow mapping
- Physically Based Bloom

---

## Dependencies

Third-party libraries are included as Git submodules:

| Library | Purpose |
|---------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | Window management and input |
| [ImGui](https://github.com/ocornut/imgui) | UI (docking branch) |
| [DirectXTex](https://github.com/microsoft/DirectXTex) | Texture loading and processing |
| [stb_image](https://github.com/nothings/stb) | Image loading |
| [WinPixEventRuntime](https://github.com/microsoft/Pix) | GPU profiling |

Build toolchain: **CMake 3.20+**, **Python 3.13+**, **LLVM/Clang** (or **Clang-cl** on Windows).

---

## Build Instructions (Windows)

> Linux support is experimental (Vulkan backend in progress).

```bash
# 1. Clone with all submodules (IMPORTANT!)
git clone --recursive https://github.com/almostcomin/AlmostEngine.git
cd AlmostEngine

# 2. Build third-party dependencies (first time only)
python BuildScripts/Build3rdParties.py --target {win-dx12-clang,linux-vk-clang} [--config {Debug,Release,RelWithDebInfo,All}]

# 3. Generate Visual Studio solution (optional, for IDE development)
python GenerateVSProjects.py --target win-dx12-clang

# 4. Alternatively, build directly from the command line:
python BuildEngine.py --target win-dx12-clang [--config {Debug,Release,RelWithDebInfo,All}]
```
---
## Future Work
- MBOIT
- Bloom in compute
- DXR / Raytracing
- GPU Driven Rendering
- Image Based Lighting
- Cascaded Shadow Maps
- Depth of Field
