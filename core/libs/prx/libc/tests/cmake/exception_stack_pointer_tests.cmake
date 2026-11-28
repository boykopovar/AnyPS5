if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    enable_language(ASM)
    add_test_executable(exception_stack_pointer_tests
        ${LIBS_INCLUDE_DIR}/tests/ExceptionStackPointer.cpp
        ${LIBS_INCLUDE_DIR}/tests/ExceptionStackPointer.S)
    target_include_directories(exception_stack_pointer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(exception_stack_pointer_tests PRIVATE libc)
    add_test(NAME exception_stack_pointer COMMAND exception_stack_pointer_tests)
    set_tests_properties(exception_stack_pointer PROPERTIES TIMEOUT 10)
endif()
