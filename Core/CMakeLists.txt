cmake_minimum_required(VERSION 3.16)

project(Core)

file(GLOB_RECURSE CORE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.*"
)

add_library(Core STATIC ${CORE_SRC})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CORE_SRC})

target_include_directories(Core 
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/Src"
)

# Third-party (GLM)
target_include_directories(Core PUBLIC
    "${CMAKE_SOURCE_DIR}/3rdParty/Sources/glm"
)

target_precompile_headers(Core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/Core/CorePCH.h"
)

target_compile_features(Core PUBLIC cxx_std_23)
set_target_properties(Core PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)