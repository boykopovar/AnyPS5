add_library(libSceVdecCore.native SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVdecCore.native PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVdecCore.native)

target_link_libraries(libSceVdecCore.native PRIVATE libSceVdecCore)

if(BUILD_TESTING)
    add_test_executable(guest_vdec_core_native_tests ${LIBS_INCLUDE_DIR}/tests/GuestVdecCoreNative.cpp)
    target_include_directories(guest_vdec_core_native_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vdec_core_native_tests PRIVATE libSceVdecCore.native libSceVdecCore libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_vdec_core_native_tests)
    add_test(NAME guest_vdec_core_native COMMAND guest_vdec_core_native_tests)
    if(WIN32)
        set_tests_properties(guest_vdec_core_native PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVdecCore.native>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVdecCore>")
    endif()
endif()
