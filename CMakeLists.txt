cmake_minimum_required(VERSION 3.0)

project(simple-signal)

include (CTest)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_subdirectory(tests)
endif()
