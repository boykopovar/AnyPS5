add_library(libSceAddressSanitizer SHARED EXCLUDE_FROM_ALL
    Allocator.cpp
    Control.cpp
    Export.cpp
    Memory.cpp
    Report.cpp
    Shadow.cpp
    Ubsan.cpp
)
target_include_directories(libSceAddressSanitizer PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAddressSanitizer)

if(BUILD_TESTING)
    add_test_executable(guest_address_sanitizer_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizer.cpp)
    target_include_directories(guest_address_sanitizer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_address_sanitizer_tests PRIVATE libSceAddressSanitizer libc)
    configure_windows_unwind(guest_address_sanitizer_tests)
    add_test(NAME guest_address_sanitizer COMMAND guest_address_sanitizer_tests)
    set_tests_properties(guest_address_sanitizer PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_address_sanitizer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
    endif()
endif()
