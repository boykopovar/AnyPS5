add_library(libSceSysmodule SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSysmodule PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSysmodule)

if(BUILD_TESTING)
    add_test_executable(guest_sysmodule_tests ${LIBS_INCLUDE_DIR}/tests/GuestSysmodule.cpp)
    target_include_directories(guest_sysmodule_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_sysmodule_tests PRIVATE libSceSysmodule libkernel libc)
    configure_windows_unwind(guest_sysmodule_tests)
    set(guestSysmoduleRun ${CMAKE_CURRENT_BINARY_DIR}/guest_sysmodule)
    file(MAKE_DIRECTORY ${guestSysmoduleRun}/libs)
    add_custom_target(guest_sysmodule_runtime
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:libSceFiber>" "${guestSysmoduleRun}/libs/libSceFiber.prx"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:libSceAudioPropagation>" "${guestSysmoduleRun}/libs/libSceAudioPropagation.prx"
            VERBATIM
    )
    add_dependencies(guest_sysmodule_runtime libSceFiber libSceAudioPropagation)
    add_dependencies(guest_sysmodule_tests guest_sysmodule_runtime)
    add_test(NAME guest_sysmodule COMMAND guest_sysmodule_tests)
    set_tests_properties(guest_sysmodule PROPERTIES WORKING_DIRECTORY ${guestSysmoduleRun})
    if(UNIX AND NOT APPLE)
        add_test(NAME guest_sysmodule_maps_scan COMMAND guest_sysmodule_tests)
        set_tests_properties(guest_sysmodule_maps_scan PROPERTIES ENVIRONMENT "APS5_NO_PROCMAP_QUERY=1"
            WORKING_DIRECTORY ${guestSysmoduleRun})
    endif()
    if(WIN32)
        set_tests_properties(guest_sysmodule PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSysmodule>")
    endif()
    add_test_executable(guest_sysmodule_invalid_ids_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/InvalidIds.cpp)
    target_include_directories(guest_sysmodule_invalid_ids_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_sysmodule_invalid_ids_tests PRIVATE libSceSysmodule libkernel libc)
    configure_windows_unwind(guest_sysmodule_invalid_ids_tests)
    add_test(NAME guest_sysmodule-invalid-ids COMMAND guest_sysmodule_invalid_ids_tests)
    if(WIN32)
        set_tests_properties(guest_sysmodule-invalid-ids PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSysmodule>")
    endif()
endif()
