if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    enable_language(ASM)
    add_test_executable(guest_unwind_enclosing_bounds_tests
        ${LIBS_INCLUDE_DIR}/tests/GuestUnwindEnclosingBounds.cpp
        ${LIBS_INCLUDE_DIR}/tests/GuestUnwindEnclosingBounds.S)
    target_include_directories(guest_unwind_enclosing_bounds_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_unwind_enclosing_bounds_tests PRIVATE libc)
    add_test(NAME guest_unwind_enclosing_bounds COMMAND guest_unwind_enclosing_bounds_tests)
    set_tests_properties(guest_unwind_enclosing_bounds PROPERTIES TIMEOUT 10)
endif()
