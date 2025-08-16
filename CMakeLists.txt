cmake_minimum_required(VERSION 3.16)
project(Structure)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# 3rd parties
set(SDL3_DIR "${CMAKE_SOURCE_DIR}/3rdParty/SDL/Debug/cmake")
find_package(SDL3 REQUIRED)

# Framework projects
#add_subdirectory(Framework/Core)
#add_subdirectory(Framework/Video)
#add_subdirectory(Framework/Input)

add_executable(Structure Src/main.cpp)

# Vincular con los módulos
#target_link_libraries(MyGame
#    PRIVATE
#    Core
#    Video
#    Input
#)

# Incluir cabeceras internas
#target_include_directories(MyGame
#    PRIVATE
#    Framework/Core
#    Framework/Video
#    Framework/Input
#)

target_include_directories(Structure PRIVATE "${CMAKE_SOURCE_DIR}/3rdParty/SDL3/include")

# Link to the actual SDL3 library
target_link_libraries(Structure PRIVATE SDL3::SDL3)
