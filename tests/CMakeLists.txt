add_executable(signalTests Tests.cpp)

target_link_libraries(signalTests gtest gtest_main)
gtest_discover_tests(signalTests)
