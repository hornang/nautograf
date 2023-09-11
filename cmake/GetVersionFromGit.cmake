find_package(Git REQUIRED)

function(get_version_from_git)
    set(oneValueArgs SEMANTIC DESCRIBE)
    cmake_parse_arguments(GET_VERSION "" "${oneValueArgs}" "" ${ARGN})

    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
                    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT GIT_DESCRIBE_ERROR_CODE)
        string(REGEX MATCH "^v(([0-9]+\.[0-9]+\.[0-9]+).*$)" MATCH_OUTPUT ${GIT_DESCRIBE_VERSION})
        if (NOT ${MATCH_OUTPUT} STREQUAL "")
            set(DESCRIBE ${CMAKE_MATCH_1})
            set(SEMANTIC ${CMAKE_MATCH_2})
        else()
            message(WARNING "Failed to parse tag " ${GIT_DESCRIBE_VERSION})
        endif()
    else()
        message(WARNING "Failed to run git command " ${GIT_DESCRIBE_VERSION} "Error code " ${GIT_DESCRIBE_ERROR_CODE})
    endif()

    if (NOT DEFINED SEMANTIC OR NOT DEFINED DESCRIBE)
        message(WARNING "Failed to determine VERSION_STRING from Git tags. Using default version 0.0.0")
        set(SEMANTIC "0.0.0")
        set(DESCRIBE "0.0.0-unknown")
    endif()

    set(${GET_VERSION_SEMANTIC} ${SEMANTIC} PARENT_SCOPE)
    set(${GET_VERSION_DESCRIBE} ${DESCRIBE} PARENT_SCOPE)
endfunction()

