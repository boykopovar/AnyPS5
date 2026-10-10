add_library(libScePngEnc SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libScePngEnc PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePngEnc PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePngEnc)
target_link_libraries(libScePngEnc PRIVATE decoder_png)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_png_enc_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPngEnc.cpp
        LINK libScePngEnc decoder_png libc
        RUNTIME libScePngEnc
    )
endif()
