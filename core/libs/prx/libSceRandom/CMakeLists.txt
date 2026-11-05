add_library(libSceRandom SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceRandom PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRandom)

if(BUILD_TESTING)
    add_test_executable(guest_random_tests ${LIBS_INCLUDE_DIR}/tests/GuestRandom.cpp)
    target_include_directories(guest_random_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_random_tests PRIVATE libSceRandom)
    add_test(NAME guest_random COMMAND guest_random_tests)
    if(WIN32)
        set_tests_properties(guest_random PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRandom>")
    endif()
endif()
