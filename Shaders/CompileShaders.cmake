
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

  string(REPLACE "Shaders/Src" "Data/Shaders" OUTPUT_DIR "${INPUT_FILE}")
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