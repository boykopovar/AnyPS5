add_library(libScePngEnc SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libScePngEnc PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePngEnc PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePngEnc)
target_link_libraries(libScePngEnc PRIVATE decoder_png)

if(BUILD_TESTING)
    add_test_executable(guest_encoder_delete_read_only_tests ${LIBS_INCLUDE_DIR}/tests/GuestEncoderDeleteReadOnly.cpp)
    target_include_directories(guest_encoder_delete_read_only_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_encoder_delete_read_only_tests PRIVATE libSceJpegEnc libScePngEnc libc)
    configure_windows_unwind(guest_encoder_delete_read_only_tests)
    add_test(NAME guest_encoder_delete_read_only COMMAND guest_encoder_delete_read_only_tests)
    set_tests_properties(guest_encoder_delete_read_only PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_encoder_delete_read_only PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegEnc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_png_encoder_isolation_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngEncoderIsolation.cpp)
    target_include_directories(guest_png_encoder_isolation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_encoder_isolation_tests PRIVATE libScePngEnc libc decoder_png)
    configure_windows_unwind(guest_png_encoder_isolation_tests)
    add_test(NAME guest_png_encoder_isolation COMMAND guest_png_encoder_isolation_tests)
    set_tests_properties(guest_png_encoder_isolation PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_png_encoder_isolation PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_png_enc_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngEnc.cpp)
    target_include_directories(guest_png_enc_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_enc_tests PRIVATE libScePngEnc decoder_png libc)
    configure_windows_unwind(guest_png_enc_tests)
    add_test(NAME guest_png_enc COMMAND guest_png_enc_tests)
    if(WIN32)
        set_tests_properties(guest_png_enc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>")
    endif()
endif()
if(BUILD_TESTING)
    add_test_executable(guest_png_enc_context_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngEncContext.cpp)
    target_include_directories(guest_png_enc_context_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_enc_context_tests PRIVATE libScePngEnc decoder_png libc)
    configure_windows_unwind(guest_png_enc_context_tests)
    add_test(NAME guest_png_enc_context COMMAND guest_png_enc_context_tests)
    set_tests_properties(guest_png_enc_context PROPERTIES TIMEOUT 120)
    if(WIN32)
        set_tests_properties(guest_png_enc_context PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>")
    endif()
endif()
if(BUILD_TESTING)
    add_test_executable(guest_png_input_extent_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngInputExtent.cpp)
    target_include_directories(guest_png_input_extent_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_input_extent_tests PRIVATE libScePngEnc decoder_png libc)
    configure_windows_unwind(guest_png_input_extent_tests)
    add_test(NAME guest_png_input_extent COMMAND guest_png_input_extent_tests)
    set_tests_properties(guest_png_input_extent PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_png_input_extent PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>")
    endif()
endif()

if(BUILD_TESTING)
    set(PNG_ENC_WRITES_MODULE_DIR ${CMAKE_CURRENT_BINARY_DIR}/writes-modules)
    add_custom_command(
        OUTPUT ${PNG_ENC_WRITES_MODULE_DIR}/libScePngEnc.prx
               ${PNG_ENC_WRITES_MODULE_DIR}/libc.prx
               ${PNG_ENC_WRITES_MODULE_DIR}/libkernel.prx
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PNG_ENC_WRITES_MODULE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libScePngEnc> ${PNG_ENC_WRITES_MODULE_DIR}/libScePngEnc.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${PNG_ENC_WRITES_MODULE_DIR}/libc.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${PNG_ENC_WRITES_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libc ${PNG_ENC_WRITES_MODULE_DIR}/libc.prx
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${PNG_ENC_WRITES_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libScePngEnc --preserve-export-list ${LIBC_EXPORT_LIST} ${PNG_ENC_WRITES_MODULE_DIR}/libScePngEnc.prx
        DEPENDS libScePngEnc libc libkernel nid_patcher ${LIBC_EXPORT_LIST}
        VERBATIM
    )
    add_custom_target(png_enc_writes_modules DEPENDS ${PNG_ENC_WRITES_MODULE_DIR}/libScePngEnc.prx)
    add_test_executable(guest_png_enc_writes_tests NO_RUNTIME_DLL_STAGING ${LIBS_INCLUDE_DIR}/tests/GuestPngEncWrites.cpp)
    target_include_directories(guest_png_enc_writes_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_enc_writes_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_png_enc_writes_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_png_enc_writes_tests)
    add_dependencies(guest_png_enc_writes_tests png_enc_writes_modules)
    add_test(NAME guest_png_enc_writes COMMAND guest_png_enc_writes_tests $<TARGET_FILE:libScePngEnc> raw)
    add_test(NAME guest_png_enc_writes_nid COMMAND guest_png_enc_writes_tests ${PNG_ENC_WRITES_MODULE_DIR}/libScePngEnc.prx nid)
    set_tests_properties(guest_png_enc_writes guest_png_enc_writes_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set(PNG_ENC_WRITES_PATH PATH)
    elseif(APPLE)
        set(PNG_ENC_WRITES_PATH DYLD_LIBRARY_PATH)
    else()
        set(PNG_ENC_WRITES_PATH LD_LIBRARY_PATH)
    endif()
    set_tests_properties(guest_png_enc_writes PROPERTIES ENVIRONMENT_MODIFICATION
        "${PNG_ENC_WRITES_PATH}=path_list_prepend:$<TARGET_FILE_DIR:libScePngEnc>")
    set_tests_properties(guest_png_enc_writes_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "${PNG_ENC_WRITES_PATH}=path_list_prepend:${PNG_ENC_WRITES_MODULE_DIR}")

endif()
