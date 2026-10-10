cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(RegisterContextTests LANGUAGES CXX)
    enable_testing()
    set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)
add_executable(register_context_tests ../RegisterContext.cpp)
target_include_directories(register_context_tests PRIVATE ../..)
target_compile_features(register_context_tests PRIVATE cxx_std_20)
target_link_libraries(register_context_tests PRIVATE Threads::Threads)
set_target_properties(register_context_tests PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")

foreach(mode capture restore threads)
    add_test(NAME register_context_${mode} COMMAND register_context_tests ${mode})
    set_tests_properties(register_context_${mode} PROPERTIES TIMEOUT 15)
endforeach()
