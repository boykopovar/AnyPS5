if(BUILD_TESTING)
    set(icu_resource_fixture ${CMAKE_CURRENT_BINARY_DIR}/resource-fixture/icudt78l.dat)
    add_custom_command(OUTPUT ${icu_resource_fixture}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundlePackage.py
            ${icu_host_genrb} ${icu_host_icupkg} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundleData.txt
            ${icu_data_dir}/icudt78l.dat ${icu_resource_fixture}
        DEPENDS icu_host_tools ${icu_host_genrb} ${icu_host_icupkg} ${icu_data_dir}/icudt78l_dat.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundleData.txt ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundlePackage.py
        VERBATIM)
    add_custom_target(guest_icu_resource_bundle_data DEPENDS ${icu_resource_fixture})
    add_test_executable(guest_icu_resource_bundle_native_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundleNative.cpp)
    target_link_libraries(guest_icu_resource_bundle_native_tests PRIVATE icu_common)
    configure_host_test_executable(guest_icu_resource_bundle_native_tests)
    add_dependencies(guest_icu_resource_bundle_native_tests guest_icu_resource_bundle_data)
    add_test(NAME guest_icu_resource_bundle_native_fixture COMMAND guest_icu_resource_bundle_native_tests ${icu_resource_fixture})
    set_tests_properties(guest_icu_resource_bundle_native_fixture PROPERTIES TIMEOUT 20)
    add_test_executable(guest_icu_resource_bundle_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundleBehavior.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResourceBundleOracle.cpp)
    target_include_directories(guest_icu_resource_bundle_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_resource_bundle_tests PRIVATE libSceIcu libkernel libc icu_common)
    configure_windows_unwind(guest_icu_resource_bundle_tests)
    add_dependencies(guest_icu_resource_bundle_tests guest_icu_resource_bundle_data)
    foreach(mode literals fillin status lifetime memory)
        add_test(NAME guest_icu_resource_bundle_${mode} COMMAND guest_icu_resource_bundle_tests ${icu_resource_fixture} ${mode})
        set_tests_properties(guest_icu_resource_bundle_${mode} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_icu_resource_bundle_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
        endif()
    endforeach()
endif()
