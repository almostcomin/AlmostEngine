# cmake/utils.cmake

function(setup_pch TARGET_NAME PCH_HEADER_NAME PCH_SOURCE_REL)
    if(MSVC)
        get_target_property(_sources ${TARGET_NAME} SOURCES)
        foreach(_src ${_sources})
            if(_src MATCHES "\\.cpp$" AND NOT _src MATCHES "${PCH_SOURCE_REL}")
                set_source_files_properties(${_src} PROPERTIES
                    COMPILE_FLAGS "/Yu\"${PCH_HEADER_NAME}\""
                )
            endif()
        endforeach()

        set_source_files_properties(${PCH_SOURCE_REL} PROPERTIES
            COMPILE_FLAGS "/Yc\"${PCH_HEADER_NAME}\""
        )
    endif()
endfunction()