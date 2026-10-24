cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GuestArenaTests LANGUAGES CXX)
    enable_testing()
    set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)
add_executable(guest_arena_tests ../GuestArenaHost.cpp ../../prx/libc/src/GuestArena.cpp ../../prx/libc/src/GuestWriteWatch.cpp)
target_include_directories(guest_arena_tests PRIVATE ../..)
target_compile_features(guest_arena_tests PRIVATE cxx_std_20)
target_link_libraries(guest_arena_tests PRIVATE Threads::Threads)
set_target_properties(guest_arena_tests PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")

foreach(mode available unmappable host gap commit)
    add_test(NAME guest_arena_${mode} COMMAND guest_arena_tests ${mode})
    set_tests_properties(guest_arena_${mode} PROPERTIES TIMEOUT 30)
endforeach()
