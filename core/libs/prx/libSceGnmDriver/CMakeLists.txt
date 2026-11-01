add_library(libSceGnmDriver SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceGnmDriver PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceGnmDriver PRIVATE libSceAgcDriver libSceVideoOut)
configure_windows_unwind(libSceGnmDriver)

if(BUILD_TESTING)
    add_test_executable(guest_gnm_driver_tests ${LIBS_INCLUDE_DIR}/tests/GuestGnmDriver.cpp)
    target_include_directories(guest_gnm_driver_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_gnm_driver_tests PRIVATE libSceGnmDriver libSceAgcDriver libc)
    configure_windows_unwind(guest_gnm_driver_tests)
    add_test(NAME guest_gnm_driver COMMAND guest_gnm_driver_tests)
    set_tests_properties(guest_gnm_driver PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_tests_properties(guest_gnm_driver PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriver>")
    endif()

    add_test_executable(gnm_driver_submit_tests tests/Submit.cpp)
    target_include_directories(gnm_driver_submit_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/libs/prx/libSceAgcDriver/tests/execution
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )
    target_link_libraries(gnm_driver_submit_tests PRIVATE libSceGnmDriver libSceAgcDriver SDL2-static libc)
    set_target_properties(gnm_driver_submit_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(gnm_driver_submit_tests)
    add_test(NAME gnm_driver_packets COMMAND gnm_driver_submit_tests --packets)
    set_tests_properties(gnm_driver_packets PROPERTIES TIMEOUT 20)
    add_test(NAME gnm_driver_submit_wait COMMAND gnm_driver_submit_tests)
    set_tests_properties(gnm_driver_submit_wait PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(gnm_driver_packets gnm_driver_submit_wait PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
