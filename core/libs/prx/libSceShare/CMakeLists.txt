include(${CMAKE_CURRENT_SOURCE_DIR}/ShareLibrary.cmake)
add_sce_share_library(libSceShare)

if(BUILD_TESTING)
    add_test_executable(guest_share_tests ${LIBS_INCLUDE_DIR}/tests/GuestShare.cpp)
    target_include_directories(guest_share_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_share_tests PRIVATE libSceShare)
    add_test(NAME guest_share COMMAND guest_share_tests)
    if(WIN32)
        set_tests_properties(guest_share PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceShare>")
    endif()
endif()
