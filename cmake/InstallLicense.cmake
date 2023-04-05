function(install_license)
    set(oneValueArgs FILE)
    cmake_parse_arguments(INSTALL_LICENSE "" "${oneValueArgs}" "" ${ARGN})

    if(NOT EXISTS ${INSTALL_LICENSE_FILE})
        message(FATAL_ERROR "Can not find license file ${INSTALL_LICENSE_FILE}")
    endif()

    cmake_path(GET INSTALL_LICENSE_FILE PARENT_PATH DIR)
    cmake_path(GET DIR FILENAME NAME)

    configure_file(
        ${INSTALL_LICENSE_FILE}
        "${CMAKE_BINARY_DIR}/licenses/${NAME}.txt"
        COPYONLY
    )

    install(
        FILES "${CMAKE_BINARY_DIR}/licenses/${NAME}.txt"
        DESTINATION licenses
    )

endfunction()
