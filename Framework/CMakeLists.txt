cmake_minimum_required(VERSION 3.16)
include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

project(Framework)

file(GLOB_RECURSE FRAMEWORK_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.*"
)

add_library(Framework STATIC ${FRAMEWORK_SRC})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FRAMEWORK_SRC})

target_include_directories(Framework 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/Src"
)

# PCH
setup_pch(Framework
    "Framework/FrameworkPCH.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Framework/FrameworkPCH.cpp"
)

# Dependencies
target_link_libraries(Framework PUBLIC Core RHI Gfx)
target_link_libraries(Framework PUBLIC winmm imm32 version setupapi)
target_link_libraries(Framework PUBLIC dxgi dxguid d3d12)

target_compile_features(Framework PUBLIC cxx_std_23)
set_target_properties(Framework PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
