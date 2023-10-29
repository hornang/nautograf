cmake_minimum_required(VERSION 3.21)

include(cmake/GetVersionFromGit.cmake)
get_version_from_git(
    SEMANTIC NAUTOGRAF_SEMVER
    DESCRIBE NAUTOGRAF_VERSION_DESCRIBE
)

project(nautograf LANGUAGES CXX VERSION ${NAUTOGRAF_SEMVER})

include(cmake/InstallLicense.cmake)
install_license(FILE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.txt.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.txt @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6
    COMPONENTS
    REQUIRED Gui Core Network Concurrent Quick Svg QuickControls2 ShaderTools
    CMAKE_FIND_ROOT_PATH_BOTH
)

function(AddMdfgen)
    # Used for creating signed distance field for symbols. Only the traditional
    # single channel mode is used.
    set(BUILD_SHARED_LIBS OFF)
    set(MSDFGEN_BUILD_STANDALONE false)
    add_subdirectory(msdfgen)
    install_license(FILE ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/LICENSE.txt)
endfunction()

AddMdfgen()

find_program(IMAGEMAGICK
    NAMES magick convert
)

find_program(RSVG-CONVERT
    NAMES rsvg-convert
)

find_package(UnixCommands)

add_subdirectory(cutlines)
add_subdirectory(oesenc)
add_subdirectory(src/tilefactory)

option(RESOURCE_DEV "Load QML from file system not from Qt resource" "")

if(${RESOURCE_DEV})
    set(SYMBOLS_DIR "${CMAKE_SOURCE_DIR}/symbols")
else()
    qt_add_resources(QML_RESOURCES qml/qml.qrc)
    qt_add_resources(SYMBOL_RESOURCES symbols/symbols.qrc)
    set(SYMBOLS_DIR ":/symbols")
endif()

add_subdirectory(src/scene)

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
    src/licenses.h
    src/licenses.cpp
    src/mercatorwrapper.h
    src/maptile.h
    src/maptile.cpp
    src/maptilemodel.h
    src/maptilemodel.cpp
    src/chartmodel.cpp
    src/chartmodel.h
    src/tileinfobackend.cpp
    src/tileinfobackend.h
    src/usersettings.cpp
    src/usersettings.h
    ${QML_RESOURCES}
    ${SYMBOL_RESOURCES}
    ${APP_ICON_RESOURCE_WINDOWS}
)

if(NOT ${APP_ICON_RESOURCE_WINDOWS} STREQUAL "")
    qt_add_resources(nautograf "icon"
        PREFIX "/"
        BASE ${CMAKE_BINARY_DIR}
        FILES ${CMAKE_BINARY_DIR}/icon.ico
    )
endif()

option(USE_OEXSERVERD "Use oexserverd to decrypt chart catalogs" ON)

if(${USE_OEXSERVERD})
    target_compile_definitions(nautograf PRIVATE USE_OEXSERVERD)
endif()

target_include_directories(nautograf PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)
target_include_directories(nautograf PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(nautograf PROPERTIES AUTOMOC TRUE)

if(${RESOURCE_DEV})
    target_compile_definitions(nautograf PRIVATE QML_DIR="${CMAKE_SOURCE_DIR}/qml")
endif()

target_include_directories(nautograf PRIVATE src)

target_link_libraries(nautograf
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Svg
    Qt::Gui
    tilefactory
    scene
)

install(
    TARGETS nautograf scene tilefactory oesenc
    RUNTIME_DEPENDENCY_SET dependency_set
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
)

install(
    IMPORTED_RUNTIME_ARTIFACTS nautograf
    RUNTIME_DEPENDENCY_SET dependency_set
)

install(
    RUNTIME_DEPENDENCY_SET dependency_set
    PRE_INCLUDE_REGEXES "tinyxml2" "freetype" "bz2" "brotli" "libpng" "zlib"
    PRE_EXCLUDE_REGEXES ".*"

    # tinyxml exists as runtime dependency for more than one target. Exclude
    # one of them to avoid conflict during install.
    POST_EXCLUDE_REGEXES ".*/scene/.*tinyxml2.*"
)

if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")

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

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml
        DESTINATION .
    )

    # Replace this if statement with a more explicit relation between the variable
    # beeing checked and the existence of the PNG files.
    if(NOT ${APP_ICON_RESOURCE_WINDOWS} STREQUAL "")
        install(
            FILES
                ${CMAKE_CURRENT_BINARY_DIR}/logo_44.png
                ${CMAKE_CURRENT_BINARY_DIR}/logo_150.png
            DESTINATION .
        )
    endif()
endif()

set(CPACK_GENERATOR "ZIP")
include(CPack)
