find_package(Qt6
    COMPONENTS Core Concurrent Quick ShaderTools
    REQUIRED
)

if(NOT DEFINED SYMBOLS_DIR)
    message(FATAL_ERROR "SYMBOLS_DIR not defined")
endif()

add_library(scene
    annotations/annotationmaterial.cpp
    annotations/annotationmaterial.h
    annotations/annotationnode.cpp
    annotations/annotationnode.h
    annotations/annotationshader.cpp
    annotations/annotationshader.h
    annotations/fontimage.cpp
    include/scene/annotations/fontimage.h
    include/scene/annotations/types.h
    annotations/symbolimage.cpp
    annotations/symbolimage.h
    annotations/annotater.cpp
    annotations/annotater.h

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

    geometrynode.cpp
    geometrynode.h
    materialcreator.cpp
    materialcreator.h
    rootnode.cpp
    rootnode.h

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
        "annotations/annotation.vert"
        "annotations/annotation.frag"
        "polygon/polygon.frag"
        "polygon/polygon.vert"
        "line/line.frag"
        "line/line.vert"
)

include(GenerateExportHeader)
generate_export_header(scene)

target_compile_definitions(scene PRIVATE
    SYMBOLS_DIR="${SYMBOLS_DIR}"
    APP_VERSION="${NAUTOGRAF_VERSION_DESCRIBE}"
)

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
    msdf-atlas-gen
    msdf-atlas-read
)

target_link_libraries(scene PRIVATE
    Qt6::Concurrent
    msdfgen-core
    msdfgen-ext
    tilefactory
    cutlines
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
