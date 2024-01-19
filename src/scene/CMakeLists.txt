find_package(Qt6
    COMPONENTS Core Concurrent Quick ShaderTools
    REQUIRED
)

# find_package for freetype shouldn't be necessary, but it suddenly became
# required on the ubuntu-20.04 runner on GitHub Actions. Without it the
# find_package for freetype-gl failed because it couldn't find zlib::zlib.
#
# The problem started ocurring after GitHub's 20231204 Image Update:
# https://github.com/actions/runner-images/pull/8965 which means it could be
# related.
#
# freetype-gl should be replaced with https://github.com/Chlumsky/msdf-atlas-gen
# because of several reasons. So I didn't spend more time investing the
# root cause.
find_package(freetype CONFIG REQUIRED)

find_package(freetype-gl CONFIG REQUIRED)
install_license(FILE ${freetype-gl_DIR}/copyright)

if(NOT DEFINED SYMBOLS_DIR)
    message(FATAL_ERROR "SYMBOLS_DIR not defined")
endif()

add_library(scene
    annotation/annotationmaterial.cpp
    annotation/annotationmaterial.h
    annotation/annotationnode.cpp
    annotation/annotationnode.h
    annotation/annotationshader.cpp
    annotation/annotationshader.h

    line/linematerial.cpp
    line/linematerial.h
    line/linenode.cpp
    line/linenode.h
    line/lineshader.cpp
    line/lineshader.h

    polygon/polygonmaterial.cpp
    polygon/polygonmaterial.h
    polygon/polygonnode.cpp
    polygon/polygonnode.h
    polygon/polygonshader.cpp
    polygon/polygonshader.h

    fontimage.cpp
    include/scene/fontimage.h

    geometrynode.cpp
    geometrynode.h
    rootnode.cpp
    rootnode.h
    symbolimage.cpp
    symbolimage.h

    scene.cpp
    include/scene/scene.h

    tessellator.cpp
    tessellator.h

    tilefactorywrapper.cpp
    include/scene/tilefactorywrapper.h

    tiledata.h
)

qt6_add_shaders(scene "shaders"
    PREFIX
        "scene"
    FILES
        "annotation/annotation.vert"
        "annotation/annotation.frag"
        "polygon/polygon.frag"
        "polygon/polygon.vert"
        "line/line.frag"
        "line/line.vert"
)

include(GenerateExportHeader)
generate_export_header(scene)

target_compile_definitions(scene PRIVATE SYMBOLS_DIR="${SYMBOLS_DIR}")

set_target_properties(scene PROPERTIES AUTOMOC TRUE)

target_include_directories(scene
    PRIVATE
        .
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        include
)

target_link_libraries(scene PUBLIC
    Qt6::Core
    Qt6::Quick
)

target_link_libraries(scene PRIVATE
    Qt6::Concurrent
    freetype-gl
    msdfgen-core
    msdfgen-ext
    tilefactory
    cutlines
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
