# Third-party licenses

Index of every third-party component used by the engine, with its license.
Update the vendored copy in this folder whenever you update the corresponding
submodule or binary.

## Libraries (git submodules, vendored license copy here)

| Component | License | License copy | Lives in |
|---|---|---|---|
| SDL3 | Zlib | `SDL.txt` | `3rdParty/Sources/SDL` |
| Dear ImGui | MIT | `Dear-ImGui.txt` | `3rdParty/Sources/imgui` |
| glm | Happy Bunny (modified MIT) + MIT | `glm.txt` | `3rdParty/Sources/glm` |
| cgltf | MIT | `cgltf.txt` | `3rdParty/Sources/cgltf` |
| DirectXTex | MIT | `DirectXTex.txt` | `3rdParty/Sources/DirectXTex` |
| stb | MIT or public domain (dual) | `stb.txt` | `3rdParty/Sources/stb` |
| DirectX-Headers | MIT | `DirectX-Headers.txt` | `3rdParty/Sources/DirectX-Headers` |
| Tracy Profiler | BSD 3-clause | `Tracy.txt` | `3rdParty/Sources/tracy` |
| KTX-Software (libktx) | Apache 2.0 (+ other OSS in the repo, see their LICENSE.md) | `KTX-Software.txt` + `KTX-Software-Apache-2.0.txt` | `3rdParty/Sources/KTX-Software` |

## Binaries (downloaded, license copied here)

| Component | License | License copy | Lives in |
|---|---|---|---|
| DirectX Shader Compiler (DXC) | LLVM/NCSA + MIT + MS | `DXC-LICENSE-LLVM.txt`, `DXC-LICENSE-MIT.txt`, `DXC-LICENSE-MS.txt` | `3rdParty/dxc` (downloaded at configure time by `RHI/dxc.cmake`, not versioned) |
| WinPixEventRuntime | MS EULA | `WinPixEventRuntime-license.txt`, `WinPixEventRuntime-ThirdPartyNotices.txt` | `3rdParty/WinPixEventRuntime` |

## Vendored source code (inside the engine tree)

| Component | License | License copy | Lives in |
|---|---|---|---|
| ImGuizmo (gizmos, sequencer, curve/gradient editors) | MIT | `ImGuizmo.txt` | `Framework/Src/Framework/UI/ImGuizmo` |
| ImPlot (plots) | MIT | `ImPlot.txt` | `Framework/Src/Framework/UI/ImPlot` |
| Filament libs/ibl (IBLBaker bake algorithms) | Apache 2.0 | `Filament-ibl.txt` | `Gfx/Include/Gfx/IBLBaker.h`, `Gfx/Src/Gfx/IBLBaker.cpp` |

## Assets

| Component | License | License copy | Lives in |
|---|---|---|---|
| HDR environment panoramas | CC0 (Poly Haven) + credit required (USC ICT) | `HDR-Panoramas.md` | `Apps/AlmostViewer/Data/IBL` |
