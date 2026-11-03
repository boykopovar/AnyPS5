cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(EdgeWatchTests LANGUAGES CXX)
    enable_testing()
    set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)
add_executable(edge_watch_tests ../EdgeWatch.cpp ../../prx/libkernel/Equeue/src/EdgeWatch.cpp)
target_include_directories(edge_watch_tests PRIVATE ../.. ../../prx/libc/include)
target_compile_features(edge_watch_tests PRIVATE cxx_std_20)
target_link_libraries(edge_watch_tests PRIVATE Threads::Threads)
set_target_properties(edge_watch_tests PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")

foreach(mode socket pipe write lifecycle errors)
    add_test(NAME edge_watch_${mode} COMMAND edge_watch_tests ${mode})
    set_tests_properties(edge_watch_${mode} PROPERTIES TIMEOUT 30)
endforeach()
