if(${USE_XDG_FILE_DIALOG})
    add_subdirectory(xdg-file-dialog)
endif()

set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_USE_SKIA OFF)
set(MSDFGEN_INSTALL ON)
set(MSDFGEN_DYNAMIC_RUNTIME ON)

add_subdirectory(msdfgen)
install_license(FILE ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/LICENSE.txt)

set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON)
set(MSDF_ATLAS_NO_ARTERY_FONT ON)
set(MSDF_ATLAS_INSTALL ON)
set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_DYNAMIC_RUNTIME ON)

add_subdirectory(msdf-atlas-gen)
install_license(FILE ${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen/LICENSE.txt)

add_subdirectory(msdf-atlas-read)
