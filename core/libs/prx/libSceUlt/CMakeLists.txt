add_library(libSceUlt SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceUlt PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceUlt)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ult_queue_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUltQueue.cpp
        LINK libSceUlt libc
    )
    anyps5_add_library_test(guest_ult_thread_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUltThread.cpp
        LINK libSceUlt libc
    )
endif()
