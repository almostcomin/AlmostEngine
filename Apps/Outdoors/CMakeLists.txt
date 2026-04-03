cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/*.*"
)

file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.*"
)
foreach(shader ${SHADER_FILES})
    set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
endforeach()

add_executable(Outdoors WIN32 ${SRC_FILES} ${SHADER_FILES})

if(MSVC)
    foreach(hlsl_file ${HLSL_FILES})
        set_source_files_properties(${hlsl_file} PROPERTIES
            VS_TOOL_OVERRIDE "None"
            HEADER_FILE_ONLY "TRUE"
        )
    endforeach()
endif()

# Set target properties
set_target_properties(Outdoors PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data"
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME "Outdoors"
)

target_include_directories(Outdoors PRIVATE
    "${CMAKE_SOURCE_DIR}/Src"
)

target_link_libraries(Outdoors PRIVATE Framework)

target_compile_definitions(Outdoors PRIVATE
    SHADERS_SRC_FOLDER="${CMAKE_SOURCE_DIR}/Shaders"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHADER_FILES})

# Copy bins
add_custom_command(
    TARGET Outdoors POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/dxc/bin/x64/dxcompiler.dll"
        $<TARGET_FILE_DIR:Outdoors>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/dxc/bin/x64/dxil.dll"
        $<TARGET_FILE_DIR:Outdoors>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/3rdParty/WinPixEventRuntime/bin/x64/WinPixEventRuntime.dll"
        $<TARGET_FILE_DIR:Outdoors>
)