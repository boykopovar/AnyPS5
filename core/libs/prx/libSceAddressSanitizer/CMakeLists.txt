add_library(libSceAddressSanitizer SHARED EXCLUDE_FROM_ALL Export.cpp)
if(WIN32)
    target_sources(libSceAddressSanitizer PRIVATE src/ShadowWindows.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(libSceAddressSanitizer PRIVATE src/ShadowLinux.cpp)
endif()
target_include_directories(libSceAddressSanitizer PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAddressSanitizer)

if(BUILD_TESTING)
    add_test_executable(guest_addresssanitizer_shadow_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerShadow.cpp)
    target_include_directories(guest_addresssanitizer_shadow_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_addresssanitizer_shadow_tests PRIVATE libSceAddressSanitizer libc)
    configure_windows_unwind(guest_addresssanitizer_shadow_tests)
    add_test(NAME guest_addresssanitizer_shadow COMMAND guest_addresssanitizer_shadow_tests)
    set_tests_properties(guest_addresssanitizer_shadow PROPERTIES TIMEOUT 20)
    if(WIN32)
        add_test_executable(guest_addresssanitizer_mapping_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerMapping.cpp)
        target_include_directories(guest_addresssanitizer_mapping_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_addresssanitizer_mapping_tests PRIVATE libSceAddressSanitizer libc)
        configure_windows_unwind(guest_addresssanitizer_mapping_tests)
        add_test(NAME guest_addresssanitizer_mapping COMMAND guest_addresssanitizer_mapping_tests)
        add_test(NAME guest_addresssanitizer_mapping_collision COMMAND guest_addresssanitizer_mapping_tests collision)
        set_tests_properties(guest_addresssanitizer_mapping guest_addresssanitizer_mapping_collision PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        foreach(mode load4 load16 storeN unaligned partial report_error)
            add_test(NAME guest_addresssanitizer_report_${mode} COMMAND guest_addresssanitizer_mapping_tests report ${mode})
            set_tests_properties(guest_addresssanitizer_report_${mode} PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        endforeach()
        set_tests_properties(guest_addresssanitizer_report_load4 PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 4 at 0x[0-9a-f]+ thread T-1.Shadow bytes around the buggy address:.*=>0x[0-9a-f]+:.*f7.*Shadow byte legend.*callback present=1 type=0 size=4 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_report_load16 PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address .*READ of size 16 at .*callback present=1 type=0 size=16 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_report_storeN PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address .*WRITE of size 16 at .*callback present=1 type=1 size=16 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_report_unaligned PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address .*WRITE of size 4 at .*callback present=1 type=1 size=4 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_report_partial PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address .*READ of size 1 at .*callback present=1 type=0 size=1 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_report_report_error PROPERTIES PASS_REGULAR_EXPRESSION
            "ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x000000001000 bp 0x000000002000 sp 0x000000003000.WRITE of size 2 at .*callback present=1 type=1 size=2 desc=use-after-poison address=ok text=ok.ABORTING.death callback")
        set_tests_properties(guest_addresssanitizer_shadow PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
    endif()
endif()
