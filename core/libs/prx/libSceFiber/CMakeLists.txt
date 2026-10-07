add_library(libSceFiber SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceFiber PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceFiber)

if(BUILD_TESTING)
    add_test_executable(fiber_tests ${LIBS_INCLUDE_DIR}/tests/Fiber.cpp)
    target_include_directories(fiber_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(fiber_tests PRIVATE libSceFiber libc)
    add_test(NAME fiber COMMAND fiber_tests)
    if(WIN32)
        set_tests_properties(fiber PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFiber>")
    endif()
endif()
