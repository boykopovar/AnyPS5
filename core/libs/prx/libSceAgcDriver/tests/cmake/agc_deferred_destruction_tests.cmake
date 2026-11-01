add_test_executable(agc_deferred_destruction_tests EXCLUDE_FROM_ALL tests/DeferredDestruction.cpp)
target_include_directories(agc_deferred_destruction_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_deferred_destruction_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_deferred_destruction_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_deferred_destruction_tests)

if(BUILD_TESTING)
    set_target_properties(agc_deferred_destruction_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_deferred_destruction COMMAND agc_deferred_destruction_tests)
    if(WIN32)
        set_tests_properties(agc_deferred_destruction PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
