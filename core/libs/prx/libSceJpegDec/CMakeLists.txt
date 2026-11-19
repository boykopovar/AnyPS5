if(NOT TARGET decoder_jpeg)
    add_jpeg_decoder(decoder_jpeg libc)
endif()

add_library(libSceJpegDec SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceJpegDec PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceJpegDec PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceJpegDec)
target_link_libraries(libSceJpegDec PRIVATE decoder_jpeg)

if(BUILD_TESTING)
    set(JPEG_DEC_DELETE_MODULE_DIR ${CMAKE_CURRENT_BINARY_DIR}/delete-modules)
    add_custom_command(
        OUTPUT ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx
               ${JPEG_DEC_DELETE_MODULE_DIR}/libc.prx
               ${JPEG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND ${CMAKE_COMMAND} -E make_directory ${JPEG_DEC_DELETE_MODULE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceJpegDec> ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${JPEG_DEC_DELETE_MODULE_DIR}/libc.prx
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${JPEG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libc ${JPEG_DEC_DELETE_MODULE_DIR}/libc.prx
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${JPEG_DEC_DELETE_MODULE_DIR}/libkernel.prx
        COMMAND $<TARGET_FILE:nid_patcher> libSceJpegDec --preserve-export-list ${LIBC_EXPORT_LIST} ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx
        DEPENDS libSceJpegDec libc libkernel nid_patcher ${LIBC_EXPORT_LIST}
        VERBATIM
    )
    add_custom_target(jpeg_dec_delete_modules DEPENDS ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx)
    add_test_executable(guest_jpeg_dec_delete_tests NO_RUNTIME_DLL_STAGING ${LIBS_INCLUDE_DIR}/tests/GuestJpegDecDelete.cpp)
    target_include_directories(guest_jpeg_dec_delete_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jpeg_dec_delete_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_jpeg_dec_delete_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_jpeg_dec_delete_tests)
    add_dependencies(guest_jpeg_dec_delete_tests jpeg_dec_delete_modules)
    add_test(NAME guest_jpeg_dec_delete COMMAND guest_jpeg_dec_delete_tests $<TARGET_FILE:libSceJpegDec> raw)
    add_test(NAME guest_jpeg_dec_delete_stream COMMAND guest_jpeg_dec_delete_tests $<TARGET_FILE:libSceJpegDec> raw continuation)
    add_test(NAME guest_jpeg_dec_delete_nid COMMAND guest_jpeg_dec_delete_tests ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx nid)
    add_test(NAME guest_jpeg_dec_delete_stream_nid COMMAND guest_jpeg_dec_delete_tests ${JPEG_DEC_DELETE_MODULE_DIR}/libSceJpegDec.prx nid continuation)
    set_tests_properties(guest_jpeg_dec_delete guest_jpeg_dec_delete_stream guest_jpeg_dec_delete_nid guest_jpeg_dec_delete_stream_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set(JPEG_DEC_DELETE_PATH PATH)
    elseif(APPLE)
        set(JPEG_DEC_DELETE_PATH DYLD_LIBRARY_PATH)
    else()
        set(JPEG_DEC_DELETE_PATH LD_LIBRARY_PATH)
    endif()
    set_tests_properties(guest_jpeg_dec_delete guest_jpeg_dec_delete_stream PROPERTIES ENVIRONMENT_MODIFICATION
        "${JPEG_DEC_DELETE_PATH}=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegDec>")
    set_tests_properties(guest_jpeg_dec_delete_nid guest_jpeg_dec_delete_stream_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "${JPEG_DEC_DELETE_PATH}=path_list_prepend:${JPEG_DEC_DELETE_MODULE_DIR}")

    add_test_executable(guest_jpeg_input_extent_tests ${LIBS_INCLUDE_DIR}/tests/GuestJpegInputExtent.cpp)
    target_include_directories(guest_jpeg_input_extent_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jpeg_input_extent_tests PRIVATE libSceJpegDec decoder_jpeg libc)
    configure_windows_unwind(guest_jpeg_input_extent_tests)
    add_test(NAME guest_jpeg_input_extent COMMAND guest_jpeg_input_extent_tests)
    set_tests_properties(guest_jpeg_input_extent PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_jpeg_input_extent PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegDec>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_jpeg_dec_tests ${LIBS_INCLUDE_DIR}/tests/GuestJpegDec.cpp)
    target_include_directories(guest_jpeg_dec_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jpeg_dec_tests PRIVATE libSceJpegDec decoder_jpeg libc)
    configure_windows_unwind(guest_jpeg_dec_tests)
    add_test(NAME guest_jpeg_dec COMMAND guest_jpeg_dec_tests)
    if(WIN32)
        set_tests_properties(guest_jpeg_dec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegDec>")
    endif()

    add_test_executable(guest_jpeg_dec_input_control_tests ${LIBS_INCLUDE_DIR}/tests/GuestJpegDecInputControl.cpp)
    target_include_directories(guest_jpeg_dec_input_control_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jpeg_dec_input_control_tests PRIVATE libSceJpegDec decoder_jpeg libc)
    configure_windows_unwind(guest_jpeg_dec_input_control_tests)
    add_test(NAME guest_jpeg_dec_input_control COMMAND guest_jpeg_dec_input_control_tests)
    if(WIN32)
        set_tests_properties(guest_jpeg_dec_input_control PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegDec>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_jpeg_dec_y8_tests ${LIBS_INCLUDE_DIR}/tests/GuestJpegDecY8.cpp)
    target_include_directories(guest_jpeg_dec_y8_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jpeg_dec_y8_tests PRIVATE libSceJpegDec decoder_jpeg libc)
    configure_windows_unwind(guest_jpeg_dec_y8_tests)
    add_test(NAME guest_jpeg_dec_y8 COMMAND guest_jpeg_dec_y8_tests)
    if(WIN32)
        set_tests_properties(guest_jpeg_dec_y8 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJpegDec>")
    endif()
endif()
