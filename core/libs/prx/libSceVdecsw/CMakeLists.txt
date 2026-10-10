add_library(libSceVdecsw SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVdecsw PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVdecsw)

target_link_libraries(libSceVdecsw PRIVATE libSceVideodec2)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_vdecsw_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestVdecsw.cpp
        LINK libSceVdecsw libSceVideodec2 libc
    )
endif()
