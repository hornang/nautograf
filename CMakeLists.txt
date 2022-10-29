cmake_minimum_required(VERSION 3.21)

project(nautograf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6
    COMPONENTS
    REQUIRED Gui Core Network Concurrent Quick Svg QuickControls2 ShaderTools
    CMAKE_FIND_ROOT_PATH_BOTH
)

# Used for creating signed distance field for symbols. Only the traditional
# single channel mode is used.
set(MSDFGEN_BUILD_STANDALONE false)
add_subdirectory(msdfgen)

find_package(freetype-gl CONFIG REQUIRED)

find_program(IMAGEMAGICK
    NAMES magick convert
)

find_program(RSVG-CONVERT
    NAMES rsvg-convert
)

find_package(UnixCommands)

add_subdirectory(oesenc)
add_subdirectory(src/tilefactory)

set(CLIPPER2_TESTS OFF)
set(CLIPPER2_EXAMPLES OFF)
set(CLIPPER2_TESTS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(Clipper2/CPP)

option(RESOURCE_DEV "Load QML from file system not from Qt resource" "")

if(${RESOURCE_DEV})
    set(SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/symbols")
else()
    qt_add_resources(QML_RESOURCES qml/qml.qrc)
    qt_add_resources(SYMBOL_RESOURCES symbols/symbols.qrc)
    set(SYMBOLS_DIR ":/symbols")
endif()

if (NOT ${BASH} STREQUAL "BASH-NOTFOUND"
        AND NOT ${IMAGEMAGICK} STREQUAL "IMAGEMAGICK-NOTFOUND"
        AND NOT ${RSVG-CONVERT} STREQUAL "RSVG-CONVERT-NOTFOUND")
    configure_file(
        icon.rc.in
        ${CMAKE_BINARY_DIR}/icon.rc
    )

    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_BINARY_DIR}/icon.rc")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/icon.ico
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/logo.svg ${CMAKE_CURRENT_SOURCE_DIR}/create_icon.sh
        COMMAND ${BASH} "${CMAKE_CURRENT_SOURCE_DIR}/create_icon.sh" ${CMAKE_CURRENT_SOURCE_DIR}/logo.svg ${CMAKE_BINARY_DIR}/icon.ico "${IMAGEMAGICK}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    message(WARNING "Cannot create application icon. Missing bash or imagemagick")
endif()

add_executable(nautograf WIN32
    src/main.cpp
    src/cryptreader.cpp
    src/cryptreader.h
    src/maptile.h
    src/maptile.cpp
    src/maptilemodel.h
    src/maptilemodel.cpp
    src/tilefactorywrapper.cpp
    src/tilefactorywrapper.h
    src/chartmodel.cpp
    src/chartmodel.h
    src/usersettings.cpp
    src/usersettings.h
    src/scene/annotationmaterial.cpp
    src/scene/annotationmaterial.h
    src/scene/annotationnode.cpp
    src/scene/annotationnode.h
    src/scene/annotationshader.cpp
    src/scene/annotationshader.h
    src/scene/fontimage.cpp
    src/scene/fontimage.h
    src/scene/rootnode.cpp
    src/scene/rootnode.h
    src/scene/scene.cpp
    src/scene/scene.h
    src/scene/symbolimage.cpp
    src/scene/symbolimage.h
    src/scene/tessellator.cpp
    src/scene/tessellator.h
    src/scene/tilenode.cpp
    src/scene/tilenode.h
    ${QML_RESOURCES}
    ${SYMBOL_RESOURCES}
    ${APP_ICON_RESOURCE_WINDOWS}
)

qt6_add_shaders(nautograf "shaders"
    PREFIX
        "/"
    FILES
        "src/scene/annotation.vert"
        "src/scene/annotation.frag"
)

if(NOT ${APP_ICON_RESOURCE_WINDOWS} STREQUAL "")
    qt_add_resources(nautograf "icon"
        PREFIX "/"
        BASE ${CMAKE_BINARY_DIR}
        FILES ${CMAKE_BINARY_DIR}/icon.ico
    )
endif()


target_include_directories(nautograf PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)
target_include_directories(nautograf PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(nautograf PROPERTIES AUTOMOC TRUE)

if(${RESOURCE_DEV})
    target_compile_definitions(nautograf PRIVATE QML_DIR="${CMAKE_SOURCE_DIR}/qml")
endif()

target_compile_definitions(nautograf PRIVATE SYMBOLS_DIR="${SYMBOLS_DIR}")

target_include_directories(nautograf PRIVATE src)

target_link_libraries(nautograf
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Svg
    Qt::Gui
    tilefactory
    msdfgen::msdfgen-core
    msdfgen::msdfgen-ext
    freetype-gl
)

if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")
    get_filename_component(COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AppxManifest.xml.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/deploy-qt-windows.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/deploy-qt-windows.cmake"
        @ONLY)


    set(CPACK_PRE_BUILD_SCRIPTS
        ${CMAKE_CURRENT_BINARY_DIR}/deploy-qt-windows.cmake
    )

    install(IMPORTED_RUNTIME_ARTIFACTS nautograf
        RUNTIME_DEPENDENCY_SET dependency_set
    )

    install(RUNTIME_DEPENDENCY_SET dependency_set
        PRE_INCLUDE_REGEXES "tinyxml" "freetype" "bz2" "brotli" "libpng" "zlib"
        PRE_EXCLUDE_REGEXES ".*"
    )

    set(CPACK_GENERATOR "ZIP")
    include(CPACK)
endif()
