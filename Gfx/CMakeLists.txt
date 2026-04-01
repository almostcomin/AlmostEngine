cmake_minimum_required(VERSION 3.16)
project(Gfx)

file(GLOB_RECURSE GFX_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.*"
)

file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.*"
)

foreach(shader ${SHADER_FILES})
    set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
endforeach()

add_library(Gfx STATIC ${GFX_SRC} ${SHADER_FILES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GFX_SRC})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHADER_FILES})

target_include_directories(Gfx 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/Src"
)

target_compile_definitions(Gfx PRIVATE
    SHADERS_SRC_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
)

# PCH
setup_pch(Gfx
    "Gfx/GfxPCH.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Gfx/GfxPCH.cpp"
)

# Dependencies
target_link_libraries(Gfx PUBLIC RHI)

# Third-party: CGLTF
target_include_directories(Gfx PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/Sources/cgltf")

# Third-party: SDL3
target_include_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/SDL/$<CONFIG>/include")
target_link_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/SDL/$<CONFIG>/lib")
target_link_libraries(Gfx PUBLIC SDL3-static)

# ImGUI
target_include_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/imgui/$<CONFIG>/include")
target_link_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/imgui/$<CONFIG>/lib")
target_link_libraries(Gfx PUBLIC imgui)

# DirectXTex
target_include_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/DirectXTex/$<CONFIG>/include")
target_link_directories(Gfx PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/DirectXTex/$<CONFIG>/lib")
target_link_libraries(Gfx PUBLIC DirectXTex)

# stb
target_include_directories(Gfx PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/stb/include")

target_compile_features(Gfx PUBLIC cxx_std_23)
set_target_properties(Gfx PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)