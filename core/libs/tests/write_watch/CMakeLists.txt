cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(WriteWatchTests LANGUAGES CXX)
    enable_testing()
    set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)
add_executable(write_watch_tests ../WriteWatch.cpp ../../prx/libc/src/GuestWriteWatch.cpp)
target_include_directories(write_watch_tests PRIVATE ../..)
target_compile_features(write_watch_tests PRIVATE cxx_std_20)
target_link_libraries(write_watch_tests PRIVATE Threads::Threads)
set_target_properties(write_watch_tests PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")

foreach(mode collect register host protection threads)
    add_test(NAME write_watch_${mode} COMMAND write_watch_tests ${mode})
    set_tests_properties(write_watch_${mode} PROPERTIES TIMEOUT 30)
endforeach()
