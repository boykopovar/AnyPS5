add_library(libSceFiber SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceFiber PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceFiber)

if(BUILD_TESTING)
    anyps5_add_library_test(fiber_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/Fiber.cpp
        LINK libSceFiber libc
    )
endif()
