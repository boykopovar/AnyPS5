add_library(libSceZlib SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceZlib PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib)
set_target_properties(libSceZlib PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceZlib)
target_link_libraries(libSceZlib PRIVATE jpeg_turbo)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_zlib_tests
        TIMEOUT 30
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestZlib.cpp
        LINK libSceZlib libc
    )
endif()
