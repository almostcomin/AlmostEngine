cmake_minimum_required(VERSION 3.16)

project(AlmostViewer)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/Src/*.*"
)

file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_SOURCE_DIR}/Shaders/*.*"
)

foreach(shader ${SHADER_FILES})
    set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
endforeach()

add_subdirectory(Core)
add_subdirectory(RHI)
add_subdirectory(Gfx)

add_executable(AlmostViewer WIN32 ${SRC_FILES} ${SHADER_FILES})

if(MSVC)
    foreach(hlsl_file ${HLSL_FILES})
        set_source_files_properties(${hlsl_file} PROPERTIES
            VS_TOOL_OVERRIDE "None"
            HEADER_FILE_ONLY "TRUE"
        )
    endforeach()
endif()

set_target_properties(AlmostViewer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Data"
)
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${SHADER_FILES})

target_compile_features(AlmostViewer PUBLIC cxx_std_23)
set_target_properties(AlmostViewer PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(AlmostViewer PRIVATE
    "${CMAKE_SOURCE_DIR}/Src"
)

target_link_libraries(AlmostViewer PRIVATE Core RHI Gfx)

# Copy bins
add_custom_command(
    TARGET AlmostViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/dxc/bin/x64/dxcompiler.dll"
        $<TARGET_FILE_DIR:AlmostViewer>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/dxc/bin/x64/dxil.dll"
        $<TARGET_FILE_DIR:AlmostViewer>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/WinPixEventRuntime/bin/x64/WinPixEventRuntime.dll"
        $<TARGET_FILE_DIR:AlmostViewer>
)

target_link_libraries(AlmostViewer PRIVATE winmm imm32 version setupapi)
target_link_libraries(AlmostViewer PRIVATE dxgi dxguid d3d12)

target_compile_definitions(AlmostViewer PRIVATE
    SHADERS_SRC_FOLDER="${CMAKE_SOURCE_DIR}/Shaders"
)