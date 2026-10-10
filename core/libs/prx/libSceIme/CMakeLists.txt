add_library(libSceIme SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceIme PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceIme)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ime_parameters_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestImeParameters.cpp
        LINK libSceIme libc
        RUNTIME libSceIme
    )

    anyps5_add_library_test(guest_ime_keyboard_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestImeKeyboard.cpp
        LINK libSceIme libc
        RUNTIME libSceIme
    )
endif()
