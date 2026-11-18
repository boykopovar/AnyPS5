add_library(libSceCommonDialog SHARED EXCLUDE_FROM_ALL Export.cpp InventoryExports.cpp AdditionalExports.cpp)
target_include_directories(libSceCommonDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceCommonDialog)

if(BUILD_TESTING)
    add_test_executable(guest_common_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestCommonDialog.cpp)
    target_include_directories(guest_common_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_common_dialog_tests PRIVATE libSceCommonDialog libc)
    configure_windows_unwind(guest_common_dialog_tests)
    add_test(NAME guest_common_dialog COMMAND guest_common_dialog_tests)
    if(Python3_FOUND)
        add_test(NAME guest_common_dialog_client_d0 COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/ClientD0.py $<TARGET_FILE:guest_common_dialog_tests>)
    endif()
    if(WIN32)
        set_tests_properties(guest_common_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        if(Python3_FOUND)
            set_tests_properties(guest_common_dialog_client_d0 PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
endif()
