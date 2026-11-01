add_library(libSceVdecCore SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVdecCore PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVdecCore)

target_link_libraries(libSceVdecCore PRIVATE ffmpeg)

if(BUILD_TESTING)
    add_test_executable(guest_vdec_core_tests ${LIBS_INCLUDE_DIR}/tests/GuestVdecCore.cpp)
    target_include_directories(guest_vdec_core_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vdec_core_tests PRIVATE libSceVdecCore libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_vdec_core_tests)
    add_test(NAME guest_vdec_core COMMAND guest_vdec_core_tests)
    if(WIN32)
        set_tests_properties(guest_vdec_core PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVdecCore>")
    endif()
endif()
