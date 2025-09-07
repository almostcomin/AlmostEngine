cmake_minimum_required(VERSION 3.15)

include(FetchContent)
include(CompileShaders.cmake)

# --- Get DXC ---

set(SHADERMAKE_DXC_VERSION "v1.8.2505" CACHE STRING "DXC to download from 'GitHub/DirectXShaderCompiler' releases")
set(SHADERMAKE_DXC_DATE "2025_05_24" CACHE STRING "DXC release date") # DXC releases on GitHub have this in download links :(
set(DXC_SUBSTRING "dxc_${SHADERMAKE_DXC_DATE}.zip")
set(DXC_DOWNLOAD_LINK https://github.com/microsoft/DirectXShaderCompiler/releases/download/${SHADERMAKE_DXC_VERSION}/${DXC_SUBSTRING})

if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64") OR(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
    set(WINDOWS_ARCH "arm64")
else()
    set(WINDOWS_ARCH "x64")
endif()

set(DXC_SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdParty/dxc_${SHADERMAKE_DXC_VERSION}")

FetchContent_Declare(
	dxc
    URL ${DXC_DOWNLOAD_LINK}
    SOURCE_DIR ${DXC_SOURCE_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP 1
    DOWNLOAD_NO_PROGRESS 1
)
message(STATUS "ShaderMake: downloading DXC ${SHADERMAKE_DXC_VERSION}...")

FetchContent_MakeAvailable(dxc)

set(SHADERMAKE_DXC_PATH "${dxc_SOURCE_DIR}/bin/${WINDOWS_ARCH}/dxc.exe" CACHE INTERNAL "")

# --- Compile shaders ---

file(GLOB_RECURSE SRC_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.hlsl"
)

CreateShadersTarget("${SRC_SHADERS}") 
