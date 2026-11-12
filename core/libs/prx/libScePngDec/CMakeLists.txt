add_png_decoder(decoder_png libc)

add_library(libScePngDec SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libScePngDec PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePngDec PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePngDec)
target_link_libraries(libScePngDec PRIVATE decoder_png)

if(BUILD_TESTING)
    set(PNG_DEC_DELETE_MODULE_DIR ${CMAKE_CURRENT_BINARY_DIR}/delete-modules)
    add_custom_command(
        OUTPUT ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx
               ${PNG_DEC_DELETE_MODULE_DIR}/libc.prx
               ${PNG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PNG_DEC_DELETE_MODULE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libScePngDec> ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${PNG_DEC_DELETE_MODULE_DIR}/libc.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${PNG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libc ${PNG_DEC_DELETE_MODULE_DIR}/libc.prx
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${PNG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libScePngDec --preserve-export-list ${LIBC_EXPORT_LIST} ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx
        DEPENDS libScePngDec libc libkernel nid_patcher ${LIBC_EXPORT_LIST}
        VERBATIM
    )
    add_custom_target(png_dec_delete_modules DEPENDS ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx)
    add_test_executable(guest_png_dec_delete_tests NO_RUNTIME_DLL_STAGING ${LIBS_INCLUDE_DIR}/tests/GuestPngDecDelete.cpp)
    target_include_directories(guest_png_dec_delete_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_dec_delete_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_png_dec_delete_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_png_dec_delete_tests)
    add_dependencies(guest_png_dec_delete_tests png_dec_delete_modules)
    add_test(NAME guest_png_dec_delete COMMAND guest_png_dec_delete_tests $<TARGET_FILE:libScePngDec> raw)
    add_test(NAME guest_png_dec_delete_stream COMMAND guest_png_dec_delete_tests $<TARGET_FILE:libScePngDec> raw continuation)
    add_test(NAME guest_png_dec_delete_nid COMMAND guest_png_dec_delete_tests ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx nid)
    add_test(NAME guest_png_dec_delete_stream_nid COMMAND guest_png_dec_delete_tests ${PNG_DEC_DELETE_MODULE_DIR}/libScePngDec.prx nid continuation)
    set_tests_properties(guest_png_dec_delete guest_png_dec_delete_stream guest_png_dec_delete_nid guest_png_dec_delete_stream_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set(PNG_DEC_DELETE_PATH PATH)
    elseif(APPLE)
        set(PNG_DEC_DELETE_PATH DYLD_LIBRARY_PATH)
    else()
        set(PNG_DEC_DELETE_PATH LD_LIBRARY_PATH)
    endif()
    set_tests_properties(guest_png_dec_delete guest_png_dec_delete_stream PROPERTIES ENVIRONMENT_MODIFICATION
        "${PNG_DEC_DELETE_PATH}=path_list_prepend:$<TARGET_FILE_DIR:libScePngDec>")
    set_tests_properties(guest_png_dec_delete_nid guest_png_dec_delete_stream_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "${PNG_DEC_DELETE_PATH}=path_list_prepend:${PNG_DEC_DELETE_MODULE_DIR}")

    add_test_executable(guest_png_dec_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngDec.cpp)
    target_include_directories(guest_png_dec_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_dec_tests PRIVATE libScePngDec decoder_png libc)
    configure_windows_unwind(guest_png_dec_tests)
    add_test(NAME guest_png_dec COMMAND guest_png_dec_tests)

    add_test_executable(guest_png_dec_input_control_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngDecInputControl.cpp)
    target_include_directories(guest_png_dec_input_control_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_dec_input_control_tests PRIVATE libScePngDec decoder_png libc)
    configure_windows_unwind(guest_png_dec_input_control_tests)
    add_test(NAME guest_png_dec_input_control COMMAND guest_png_dec_input_control_tests)
    if(WIN32)
        set_tests_properties(guest_png_dec_input_control PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngDec>")
    endif()
    add_test_executable(guest_png_dec_work_memory_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngDecWorkMemory.cpp)
    target_include_directories(guest_png_dec_work_memory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_dec_work_memory_tests PRIVATE libScePngDec decoder_png libc)
    configure_windows_unwind(guest_png_dec_work_memory_tests)
    add_test(NAME guest_png_dec_work_memory COMMAND guest_png_dec_work_memory_tests)

    if(WIN32)
        set_tests_properties(guest_png_dec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngDec>")
    endif()
    if(WIN32)
        set_tests_properties(guest_png_dec_work_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngDec>")
    endif()
endif()
