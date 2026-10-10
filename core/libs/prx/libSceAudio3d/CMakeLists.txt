add_library(libSceAudio3d SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceAudio3d PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAudio3d)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_audio3d_parameters_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAudio3dParameters.cpp
        LINK libSceAudio3d libc
    )
    anyps5_add_library_test(guest_audio3d_port_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAudio3dPort.cpp
        LINK libSceAudio3d libc
    )
endif()
