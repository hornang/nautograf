set(CARGO_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)
set(CARGO_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/target)

set(WRAPPER_LIB_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lib.rs)
set(BRIDGE_CPP ${CARGO_TARGET_DIR}/cxxbridge/tilefactory_rust/lib.rs.cc)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CARGO_TARGET_SUBDIR "release")
    set(CARGO_RELEASE_ARG "--release")
else()
    set(CARGO_TARGET_SUBDIR "debug")
    if(MSVC)
        set(CARGO_BUILD_ENV_PRELUDE set CFLAGS=-MDd && set CXXFLAGS=-MDd &&)
    endif()
endif()

set(WRAPPER_LIB ${CARGO_TARGET_DIR}/${CARGO_TARGET_SUBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}tilefactory_rust${CMAKE_STATIC_LIBRARY_SUFFIX})

set(CARGO_BUILD_COMMAND cargo build ${CARGO_RELEASE_ARG} --target-dir ${CARGO_TARGET_DIR} --manifest-path ${CARGO_MANIFEST})
set(CARGO_BUILD_COMMAND_WITH_ENV ${CARGO_BUILD_ENV_PRELUDE} ${CARGO_BUILD_COMMAND})

add_custom_command(
    OUTPUT ${BRIDGE_CPP} ${WRAPPER_LIB}
    COMMAND ${CARGO_BUILD_COMMAND_WITH_ENV}
    DEPENDS ${WRAPPER_LIB_SOURCE_FILE}
    USES_TERMINAL
    COMMENT "Running cargo..."
)

add_library(tilefactory-rust-bridge STATIC
    ${BRIDGE_CPP}
)

target_include_directories(tilefactory-rust-bridge
    PUBLIC
        ${CARGO_TARGET_DIR}/cxxbridge
)

set_target_properties(
    tilefactory-rust-bridge
    PROPERTIES ADDITIONAL_CLEAN_FILES ${CARGO_TARGET_DIR}
)

target_link_libraries(tilefactory-rust-bridge ${WRAPPER_LIB})

if(WIN32)
    target_link_libraries(tilefactory-rust-bridge
        userenv ws2_32 bcrypt ntdll
    )
elseif(UNIX)
    target_link_libraries(tilefactory-rust-bridge
        pthread
        dl
    )
endif()
