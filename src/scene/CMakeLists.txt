find_package(Qt6
    COMPONENTS Core Concurrent Quick ShaderTools
    REQUIRED
)

find_package(freetype-gl CONFIG REQUIRED)

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

    polygon/polygonmaterial.cpp
    polygon/polygonmaterial.h
    polygon/polygonnode.cpp
    polygon/polygonnode.h
    polygon/polygonshader.cpp
    polygon/polygonshader.h

    fontimage.cpp
    fontimage.h
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

target_link_libraries(scene PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Concurrent
    msdfgen::msdfgen-core
    msdfgen::msdfgen-ext
    freetype-gl
    tilefactory
)
