add_library(libSceAppInstUtil SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceAppInstUtil PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAppInstUtil)

if(BUILD_TESTING)
    add_test_executable(guest_app_inst_util_tests ${LIBS_INCLUDE_DIR}/tests/GuestAppInstUtil.cpp)
    target_include_directories(guest_app_inst_util_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_app_inst_util_tests PRIVATE libSceAppInstUtil libc)
    configure_windows_unwind(guest_app_inst_util_tests)
    set(appInstUtilRun ${CMAKE_CURRENT_BINARY_DIR}/guest_app_inst_util)
    file(WRITE ${appInstUtilRun}/app0/sce_sys/param.json
        "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}},"
        "\"userDefinedParam1\":0,\"userDefinedParam2\":0,\"userDefinedParam3\":0,\"userDefinedParam4\":0}")
    add_test(NAME guest_app_inst_util COMMAND guest_app_inst_util_tests WORKING_DIRECTORY ${appInstUtilRun})
    if(WIN32)
        set_tests_properties(guest_app_inst_util PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAppInstUtil>")
    endif()
endif()
