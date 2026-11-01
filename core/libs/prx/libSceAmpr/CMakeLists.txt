add_library(libSceAmpr SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceAmpr PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAmpr PRIVATE libkernel libc)
configure_windows_unwind(libSceAmpr)

if(BUILD_TESTING)
    add_test_executable(guest_ampr_tests ${LIBS_INCLUDE_DIR}/tests/GuestAmpr.cpp)
    target_include_directories(guest_ampr_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ampr_tests PRIVATE libSceAmpr libkernel libc)
    configure_windows_unwind(guest_ampr_tests)
    add_test_executable(guest_ampr_div_and_buffer_size_tests ${LIBS_INCLUDE_DIR}/tests/GuestAmprDivAndBufferSize.cpp)
    target_include_directories(guest_ampr_div_and_buffer_size_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ampr_div_and_buffer_size_tests PRIVATE libSceAmpr libkernel libc)
    configure_windows_unwind(guest_ampr_div_and_buffer_size_tests)
    add_test(NAME guest_ampr_div_and_buffer_size COMMAND guest_ampr_div_and_buffer_size_tests)
    set_tests_properties(guest_ampr_div_and_buffer_size PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_ampr_div_and_buffer_size PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
    endif()

    add_test(NAME guest_ampr_markers COMMAND guest_ampr_tests)
    if(WIN32)
        set_tests_properties(guest_ampr_markers PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
    endif()

    add_test_executable(guest_amm_placement_tests ${LIBS_INCLUDE_DIR}/tests/GuestAmmPlacement.cpp)
    target_include_directories(guest_amm_placement_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_amm_placement_tests PRIVATE libSceAmpr libkernel libc)
    configure_windows_unwind(guest_amm_placement_tests)
    add_test(NAME guest_amm_placement COMMAND guest_amm_placement_tests)
    if(WIN32)
        set_tests_properties(guest_amm_placement PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
    endif()

    add_test_executable(guest_ampr_threads_tests ${LIBS_INCLUDE_DIR}/tests/GuestAmprThreads.cpp)
    target_include_directories(guest_ampr_threads_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ampr_threads_tests PRIVATE libSceAmpr libkernel libc)
    configure_windows_unwind(guest_ampr_threads_tests)
    add_test(NAME guest_ampr_threads COMMAND guest_ampr_threads_tests)
    if(WIN32)
        set_tests_properties(guest_ampr_threads PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
    endif()

    if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        enable_language(ASM)
        add_test_executable(guest_ampr_cpp_error_tests
            ${LIBS_INCLUDE_DIR}/tests/GuestAmprCppError.cpp
            ${LIBS_INCLUDE_DIR}/tests/GuestAmprCppError.S)
        target_include_directories(guest_ampr_cpp_error_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_ampr_cpp_error_tests PRIVATE libSceAmpr libkernel libc)
        configure_windows_unwind(guest_ampr_cpp_error_tests)
        add_test(NAME guest_ampr_cpp_error COMMAND guest_ampr_cpp_error_tests)
        set_tests_properties(guest_ampr_cpp_error PROPERTIES TIMEOUT 10)
        if(WIN32)
            set_tests_properties(guest_ampr_cpp_error PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
        endif()
    endif()
endif()
