include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceShare/ShareLibrary.cmake)
add_sce_share_library(libSceShare.native)

if(BUILD_TESTING)
    add_test_executable(guest_share_native_tests ${LIBS_INCLUDE_DIR}/tests/GuestShare.cpp)
    target_include_directories(guest_share_native_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_share_native_tests PRIVATE libSceShare.native)
    add_test(NAME guest_share_native COMMAND guest_share_native_tests)
    if(WIN32)
        set_tests_properties(guest_share_native PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceShare.native>")
    endif()
endif()
