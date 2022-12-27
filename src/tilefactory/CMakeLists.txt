find_package(CapnProto CONFIG REQUIRED)
find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp")

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS chartdata.capnp)

add_library(tilefactory
    include/tilefactory/itilesource.h
    include/tilefactory/mercator.h
    include/tilefactory/georect.h
    include/tilefactory/oesenctilesource.h
    include/tilefactory/tilefactory.h
    include/tilefactory/chart.h
    include/tilefactory/chartclipper.h
    include/tilefactory/pos.h
    include/tilefactory/triangulator.h

    chart.cpp
    filehelper.cpp
    filehelper.h
    georect.cpp
    chartclipper.cpp
    mercator.cpp
    oesenctilesource.cpp
    tilefactory.cpp
    triangulator.cpp
    pos.cpp
    ${CAPNP_SRCS}
    ${CAPNP_HDRS}
)

include(GenerateExportHeader)
generate_export_header(tilefactory)

target_include_directories(tilefactory
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include

    PRIVATE
        ${EARCUT_HPP_INCLUDE_DIRS}
)

target_link_libraries(tilefactory
    CapnProto::capnp
    Clipper2
    oesenc
)
