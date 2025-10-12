
function(get_shader_output INPUT_FILE INPUT_PROFILE OUTPUT_FILE OUTPUT_PROFILE)

  get_filename_component(FILENAME_WE "${INPUT_FILE}" NAME_WE)

  set(EXT "pso")
  set(LOCAL_OUTPUT_PROFILE "${INPUT_PROFILE}")

  if(FILENAME_WE MATCHES "_vs$")
    set(EXT "vso")
    set(LOCAL_OUTPUT_PROFILE  "vs_${INPUT_PROFILE}")
  elseif(FILENAME_WE MATCHES "_ps$")
    set(EXT "pso")
    set(LOCAL_OUTPUT_PROFILE  "ps_${INPUT_PROFILE}")
  elseif(FILENAME_WE MATCHES "_cs$")
    set(EXT "cso")
    set(LOCAL_OUTPUT_PROFILE  "cs_${INPUT_PROFILE}")
  else()
        message(FATAL_ERROR "File ${INPUT_FILE} does not end with _vs, _ps, or _cs.")
  endif()    

  string(REPLACE "Src/Gfx/Shaders" "Data/Shaders" OUTPUT_DIR "${INPUT_FILE}")
  get_filename_component(OUTPUT_DIR "${OUTPUT_DIR}" DIRECTORY)


  set(${OUTPUT_FILE} "${OUTPUT_DIR}/${FILENAME_WE}.${EXT}" PARENT_SCOPE)
  set(${OUTPUT_PROFILE} "${LOCAL_OUTPUT_PROFILE}" PARENT_SCOPE)

endfunction()

function(CreateShadersTarget SHADER_SOURCE_FILES)
  
  if(NOT DEFINED SHADERMAKE_DXC_PATH)
      message(FATAL_ERROR "DXC no está disponible. Revisa que SHADERMAKE_DXC_PATH esté configurado.")
  endif()

  set(ALL_SHADER_OUTPUTS "")

  foreach(SHADER_FILE IN LISTS SHADER_SOURCE_FILES)
    message("Processing file ${SHADER_FILE}")
    get_shader_output(${SHADER_FILE} "6_0" OUTPUT_FILE TARGET_PROFILE)
    set(ENTRY_POINT "main")

    message("  Profile is ${TARGET_PROFILE}")
    message("  Entry point is ${ENTRY_POINT}")
    message("  Output file is ${OUTPUT_FILE}")

    add_custom_command(
      OUTPUT ${OUTPUT_FILE}
      COMMAND ${SHADERMAKE_DXC_PATH}
          -T ${TARGET_PROFILE}
          -E ${ENTRY_POINT}
          -Fo ${OUTPUT_FILE}
          ${SHADER_FILE}
      DEPENDS ${SHADER_FILE}
      COMMENT "Compiling shader ${SHADER_FILE} → ${OUTPUT_FILE}"
      VERBATIM
    )

    list(APPEND ALL_SHADER_OUTPUTS ${OUTPUT_FILE})    
  endforeach()

  add_custom_target(Shaders ALL 
    DEPENDS ${ALL_SHADER_OUTPUTS}
    SOURCES ${SHADER_SOURCE_FILES}
  )

  set_source_files_properties(${SHADER_SOURCE_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHADER_SOURCE_FILES})

endfunction()

function(GenShaders)

  set(SHADERMAKE_DXC_VERSION "v1.8.2505" CACHE STRING "DXC to download from 'GitHub/DirectXShaderCompiler' releases")
  set(SHADERMAKE_DXC_DATE "2025_05_24" CACHE STRING "DXC release date") # DXC releases on GitHub have this in download links :(
  set(DXC_SUBSTRING "dxc_${SHADERMAKE_DXC_DATE}.zip")
  set(DXC_DOWNLOAD_LINK https://github.com/microsoft/DirectXShaderCompiler/releases/download/${SHADERMAKE_DXC_VERSION}/${DXC_SUBSTRING})

  if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64") OR(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
      set(WINDOWS_ARCH "arm64")
  else()
      set(WINDOWS_ARCH "x64")
  endif()

  set(DXC_SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdParty/dxc_${SHADERMAKE_DXC_VERSION}")

  FetchContent_Declare(
    dxc
      URL ${DXC_DOWNLOAD_LINK}
      SOURCE_DIR ${DXC_SOURCE_DIR}
      DOWNLOAD_EXTRACT_TIMESTAMP 1
      DOWNLOAD_NO_PROGRESS 1
  )
  message(STATUS "ShaderMake: downloading DXC ${SHADERMAKE_DXC_VERSION}...")

  FetchContent_MakeAvailable(dxc)

  set(SHADERMAKE_DXC_PATH "${DXC_SOURCE_DIR}/bin/${WINDOWS_ARCH}/dxc.exe" CACHE INTERNAL "")

  # --- Compile shaders ---

  file(GLOB_RECURSE SRC_SHADERS
      "${CMAKE_CURRENT_SOURCE_DIR}/Src/Gfx/Shaders/*.hlsl"
  )

  CreateShadersTarget("${SRC_SHADERS}")   

endfunction()