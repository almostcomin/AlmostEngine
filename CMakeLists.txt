cmake_minimum_required(VERSION 3.16)

project(Structure)

include(FetchContent)
add_subdirectory(Shaders)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/Src/*.*"
)

add_executable(Structure WIN32 ${SRC_FILES})

set_target_properties(Structure PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Data"
)
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${SRC_FILES})

target_compile_features(Structure PUBLIC cxx_std_23)
set_target_properties(Structure PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(Structure PRIVATE
    "${CMAKE_SOURCE_DIR}/Src"
    "${CMAKE_SOURCE_DIR}/3rdParty/Sources/glm"
    "${CMAKE_SOURCE_DIR}/3rdParty/Sources/cgltf"
)

# GLM
file(GLOB_RECURSE GLM_HEADERS
    "${CMAKE_SOURCE_DIR}/3rdParty/Sources/glm/glm/*.hpp"
    "${CMAKE_SOURCE_DIR}/3rdParty/Sourcesglm/glm/*.inl"
)
source_group("Src/Core/glm" FILES ${GLM_HEADERS})
target_sources(Structure PRIVATE ${GLM_HEADERS})

# CGLTF
set(CGLTF_HEADERS 
    "${CMAKE_SOURCE_DIR}/3rdParty/Sources/cgltf/cgltf.h"
)
source_group("Src/Graphics/cgltf" FILES ${CGLTF_HEADERS})
target_sources(Structure PRIVATE ${CGLTF_HEADERS})

# SDL3
target_include_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/SDL/$<CONFIG>/include")
target_link_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/SDL/$<CONFIG>/lib")
target_link_libraries(Structure PRIVATE SDL3-static)

# NVRHI
target_include_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/NVRHI/$<CONFIG>/include")
target_link_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/NVRHI/$<CONFIG>/lib")
target_link_libraries(Structure PRIVATE nvrhi nvrhi_d3d12)

# ImGUI
target_include_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/imgui/$<CONFIG>/include")
target_link_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/imgui/$<CONFIG>/lib")
target_link_libraries(Structure PRIVATE imgui)

# DirectXTex
target_include_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/DirectXTex/$<CONFIG>/include")
target_link_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/DirectXTex/$<CONFIG>/lib")
target_link_libraries(Structure PRIVATE DirectXTex)

target_link_libraries(Structure PRIVATE winmm imm32 version setupapi)
target_link_libraries(Structure PRIVATE dxgi d3d12 dxguid)