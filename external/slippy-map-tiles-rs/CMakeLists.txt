set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CARGO_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)
set(CARGO_TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/target)

set(WRAPPER_LIB_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/lib.rs)
set(BRIDGE_CPP ${CARGO_TARGET_DIR}/cxxbridge/slippy_map_tiles/lib.rs.cc)
set(WRAPPER_LIB ${CARGO_TARGET_DIR}/debug/${CMAKE_STATIC_LIBRARY_PREFIX}wrapper${CMAKE_STATIC_LIBRARY_SUFFIX})

# Add a custom command that builds the rust crate and generates C++ bridge code
add_custom_command(
    OUTPUT ${BRIDGE_CPP} ${WRAPPER_LIB}
    COMMAND cargo build --manifest-path ${CARGO_MANIFEST}
    DEPENDS ${WRAPPER_LIB_SOURCE_FILE}
    USES_TERMINAL
    COMMENT "Running cargo..."
)

add_library(slippy_map_tiles STATIC
    include/slippy_map_tiles/slippy_map_tiles.h
    ${BRIDGE_CPP}
)

target_include_directories(slippy_map_tiles
    PUBLIC
        ${CARGO_TARGET_DIR}/cxxbridge
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(
    slippy_map_tiles
    PROPERTIES ADDITIONAL_CLEAN_FILES ${CARGO_TARGET_DIR}
)

target_link_libraries(slippy_map_tiles ${WRAPPER_LIB})

# Windows-only configuration
if(WIN32)
    target_link_libraries(slippy_map_tiles
        userenv ws2_32 bcrypt ntdll
    )
    set_target_properties(
        slippy_map_tiles
        PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}
    )
elseif(UNIX)
    target_link_libraries(slippy_map_tiles
        pthread
        dl
    )
endif()
