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
- WBOIT - Weighted Blended Order Independent Transparency
- Directional, point and spot lights
- Shadow mapping

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

---

## Installation

### Prerequisites

| Software | Minimum Version |
|----------|-----------------|
| **Windows** | 11 |
| **Visual Studio** | 17.14.24 (2022) |
| **CMake** | 3.20+ |
| **Python** | 3.13+ |

### Build Instructions

```bash
# 1. Clone with all submodules (IMPORTANT!)
git clone --recursive https://github.com/almostcomin/AlmostEngine.git
cd AlmostEngine

# 2. Build third-party dependencies (first time only)
cd 3rdParty
python BuildAll.py
cd ..

# 3. Configure and generate project files
mkdir Build
cd Build
cmake ..

# 4. Open AlmostEngine.sln in Visual Studio and build
```
---
## Future Work
- MBOIT (Moment Based Order Independent Transparency)
- Bloom (Physically based, compute shader optimized)
- Raytracing (DXR integration)
