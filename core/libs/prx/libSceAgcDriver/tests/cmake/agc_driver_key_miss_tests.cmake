add_test_executable(agc_driver_key_miss_tests EXCLUDE_FROM_ALL tests/KeyMiss.cpp)
target_include_directories(agc_driver_key_miss_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_key_miss_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)
set_target_properties(agc_driver_key_miss_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_key_miss_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_key_miss_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_key_miss COMMAND agc_driver_key_miss_tests)
    set_tests_properties(agc_driver_key_miss PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_key_miss PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
