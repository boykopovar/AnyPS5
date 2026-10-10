add_library(libSceLibcInternal SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceLibcInternal PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceLibcInternal PRIVATE ${CMAKE_DL_LIBS})
configure_windows_unwind(libSceLibcInternal)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_thread_atexit_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestThreadAtexit.cpp
        LINK libSceLibcInternal libkernel libc
    )
    if(NOT WIN32)
        anyps5_add_library_test(guest_force_tls_destructor_tests
            TIMEOUT 20
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestForceTlsDestructor.cpp
            LINK libSceLibcInternal libkernel libc ${CMAKE_DL_LIBS}
        )
    endif()
endif()
