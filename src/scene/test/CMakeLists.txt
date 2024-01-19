find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(fontimage_test
    fontimage_test.cpp
)

target_link_libraries(fontimage_test
    PUBLIC
        GTest::gtest
        GTest::gtest_main
        scene
)

gtest_discover_tests(fontimage_test)
