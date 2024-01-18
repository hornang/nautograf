if(${USE_XDG_FILE_DIALOG})
    add_subdirectory(xdg-file-dialog)
endif()

set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_USE_SKIA OFF)
set(MSDFGEN_INSTALL ON)

add_subdirectory(msdfgen)
install_license(FILE ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/LICENSE.txt)
