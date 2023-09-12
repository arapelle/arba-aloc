
find_or_fetch_GTest()

add_cpp_library_basic_tests(GTest::gtest_main
    HEADER_ONLY ${PROJECT_NAME}
    SOURCES
        project_version_tests.cpp
        pool_allocator_tests.cpp
)
