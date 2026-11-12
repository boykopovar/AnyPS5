cmake_minimum_required(VERSION 3.20)

project(PipeWriteTests LANGUAGES CXX)
enable_testing()

if(NOT APPLE)
    message(FATAL_ERROR "The pipe-write fixture requires macOS")
endif()

add_executable(pipe_write_tests ../PipeWrite.cpp ../../prx/libkernel/Socket/src/Udp.cpp)
target_include_directories(pipe_write_tests PRIVATE ../..)
target_compile_features(pipe_write_tests PRIVATE cxx_std_20)
target_link_options(pipe_write_tests PRIVATE LINKER:-dead_strip)
set_target_properties(pipe_write_tests PROPERTIES CXX_EXTENSIONS OFF)

foreach(mode ready full threshold eof duplicate invalid cleanup exhausted)
    add_test(NAME pipe_write_${mode} COMMAND pipe_write_tests ${mode})
    set_tests_properties(pipe_write_${mode} PROPERTIES TIMEOUT 30)
endforeach()
