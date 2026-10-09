include(${CMAKE_CURRENT_SOURCE_DIR}/PsmlLibrary.cmake)
add_sce_psml_library(libScePsml_debug)

if(BUILD_TESTING)
    add_test_executable(guest_psml_tests ${LIBS_INCLUDE_DIR}/tests/GuestPsml.cpp)
    target_include_directories(guest_psml_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_psml_tests PRIVATE libScePsml_debug)
    add_test(NAME guest_psml COMMAND guest_psml_tests)
    if(WIN32)
        set_tests_properties(guest_psml PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePsml_debug>")
    endif()
endif()
