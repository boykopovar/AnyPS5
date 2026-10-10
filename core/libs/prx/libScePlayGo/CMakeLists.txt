add_library(libScePlayGo SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libScePlayGo PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePlayGo)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_playgo_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPlayGo.cpp
        LINK libScePlayGo libc
    )
endif()
