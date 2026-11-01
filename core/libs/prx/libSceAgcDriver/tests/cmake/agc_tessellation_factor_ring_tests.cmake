add_test_executable(agc_tessellation_factor_ring_tests EXCLUDE_FROM_ALL
        tests/TessellationFactorRing.cpp
        State/src/Configuration.cpp
)
target_include_directories(agc_tessellation_factor_ring_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_tessellation_factor_ring_tests PRIVATE libc)
set_target_properties(agc_tessellation_factor_ring_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_tessellation_factor_ring_tests)

if(BUILD_TESTING)
    set_target_properties(agc_tessellation_factor_ring_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_tessellation_factor_ring COMMAND agc_tessellation_factor_ring_tests)
    if(WIN32)
        set_tests_properties(agc_tessellation_factor_ring PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
