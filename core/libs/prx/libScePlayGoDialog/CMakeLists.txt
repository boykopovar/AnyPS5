add_library(libScePlayGoDialog SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libScePlayGoDialog PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePlayGoDialog PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePlayGoDialog)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_playgo_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPlayGoDialog.cpp
        LINK libScePlayGoDialog libc
    )
endif()
