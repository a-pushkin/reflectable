cmake_minimum_required(VERSION 3.15)
project(reflectable VERSION 0.0.1)

option(REFLECTABLE_BUILD_TESTS "Build the tests" ON)

# Set C++17 standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JSON_BuildTests OFF CACHE BOOL "Don't build tests" FORCE)
add_subdirectory(extern/json)


add_library(reflectable
    src/Reflectable.cpp
)
include_directories(reflectable
    include
    extern/json/include
)

target_link_libraries(reflectable
    nlohmann_json
)

if(REFLECTABLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(extern/googletest)
    mark_as_advanced(
        BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
        gmock_build_tests gtest_build_samples gtest_build_tests
        gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
    )

    add_subdirectory(tests)
endif()