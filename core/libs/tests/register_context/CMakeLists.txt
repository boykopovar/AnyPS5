cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(RegisterContextTests LANGUAGES CXX)
    enable_testing()
    set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

find_package(Threads REQUIRED)
set(registerContextSource ${CMAKE_CURRENT_SOURCE_DIR}/../integration/RegisterContext.cpp)
set(registerContextIncludes ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(COMMAND anyps5_add_library_test)
    anyps5_add_library_test(register_context_tests
        TIMEOUT 15
        SOURCES ${registerContextSource}
        LINK Threads::Threads
        INCLUDES ${registerContextIncludes}
    )
else()
    set(testingDirectory ${CMAKE_CURRENT_SOURCE_DIR}/../../../testing)
    add_executable(register_context_tests
        ${registerContextSource}
        ${testingDirectory}/src/Test.cpp
        ${testingDirectory}/src/Main.cpp
    )
    target_include_directories(register_context_tests PRIVATE ${registerContextIncludes} ${testingDirectory}/include)
    target_compile_features(register_context_tests PRIVATE cxx_std_20)
    target_link_libraries(register_context_tests PRIVATE Threads::Threads)
    set_target_properties(register_context_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")
    add_test(NAME register_context COMMAND register_context_tests)
    set_tests_properties(register_context PROPERTIES LABELS integration TIMEOUT 15 SKIP_RETURN_CODE 77)
endif()
set_target_properties(register_context_tests PROPERTIES CXX_EXTENSIONS OFF)
