add_library(libSceRandom SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceRandom PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRandom)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_random_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestRandom.cpp
        LINK libSceRandom libc
    )
endif()
