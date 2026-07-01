cmake_minimum_required(VERSION 3.16)
include(${CMAKE_SOURCE_DIR}/CMake/Utils.cmake)

project(Core)

set(THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/3rdParty")

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

# Third-party GLM
target_include_directories(Core PUBLIC
    "${THIRDPARTY_ROOT}/Sources/glm"
)

# Third-party tracy
target_include_directories(Core PUBLIC 
    "${THIRDPARTY_ROOT}/tracy/${ALMOSTENGINE_TARGET}/$<CONFIG>/include")
target_link_directories(Core PUBLIC 
    "${THIRDPARTY_ROOT}/tracy/${ALMOSTENGINE_TARGET}/$<CONFIG>/lib")
target_link_libraries(Core PUBLIC TracyClient)

# PCH
setup_pch(Core
    "Core/CorePCH.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Core/CorePCH.cpp"
)

target_compile_features(Core PUBLIC cxx_std_23)
set_target_properties(Core PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)