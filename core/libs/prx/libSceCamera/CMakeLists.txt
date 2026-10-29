add_library(libSceCamera SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/InventoryExports.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Handles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Attributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Device.cpp
)
target_include_directories(libSceCamera PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceCamera)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_camera_tests ${LIBS_INCLUDE_DIR}/tests/GuestCamera.cpp)
    target_include_directories(guest_camera_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_camera_tests PRIVATE libSceCamera libc)
    configure_windows_unwind(guest_camera_tests)
    add_test(NAME guest_camera COMMAND guest_camera_tests)
    if(WIN32)
        set_tests_properties(guest_camera PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceCamera>")
    endif()

    add_test_executable(guest_camera_settings_tests ${LIBS_INCLUDE_DIR}/tests/GuestCameraSettings.cpp)
    target_include_directories(guest_camera_settings_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_camera_settings_tests PRIVATE libSceCamera libc)
    configure_windows_unwind(guest_camera_settings_tests)
    add_test(NAME guest_camera_settings COMMAND guest_camera_settings_tests)
    if(WIN32)
        set_tests_properties(guest_camera_settings PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceCamera>")
    endif()
endif()
