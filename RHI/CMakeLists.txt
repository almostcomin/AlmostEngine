cmake_minimum_required(VERSION 3.16)
project(RHI)

include(FetchContent)
include(dxc.cmake)

file(GLOB_RECURSE RHI_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.*"
)

add_library(RHI STATIC ${RHI_SRC})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${RHI_SRC})

target_include_directories(RHI 
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Src"
)

# PCH
setup_pch(RHI
    "RHI/RHI_PCH.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/RHI/RHI_PCH.cpp"
)

# Dependencies
target_link_libraries(RHI PUBLIC Core)

# Third-party: DirectX-Headers
target_include_directories(RHI PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/DirectX-Headers/$<CONFIG>/include")
target_link_directories(RHI PUBLIC "${CMAKE_SOURCE_DIR}/3rdParty/DirectX-Headers/$<CONFIG>/lib")
target_link_libraries(RHI PUBLIC DirectX-Guids.lib)

# Third-party: DXC
GetDXC()
target_include_directories(RHI PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/dxc/inc")
target_link_directories(RHI PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/dxc/lib/x64")
target_link_libraries(RHI PRIVATE dxcompiler.lib)

# Third-party: PIX
target_include_directories(RHI PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/WinPixEventRuntime/Include/WinPixEventRuntime")
target_link_libraries(RHI PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/WinPixEventRuntime/bin/x64/WinPixEventRuntime.lib")

target_compile_features(RHI PUBLIC cxx_std_23)
set_target_properties(RHI PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)