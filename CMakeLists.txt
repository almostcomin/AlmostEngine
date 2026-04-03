cmake_minimum_required(VERSION 3.16)

project(AlmostEngine)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(Core)
add_subdirectory(RHI)
add_subdirectory(Gfx)
add_subdirectory(Framework)
add_subdirectory(Apps/AlmostViewer)
add_subdirectory(Apps/Outdoors)