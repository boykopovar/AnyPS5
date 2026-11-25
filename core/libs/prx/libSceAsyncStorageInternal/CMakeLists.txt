add_library(libSceAsyncStorageInternal SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceAsyncStorageInternal PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAsyncStorageInternal)

if(BUILD_TESTING)
    add_test_executable(guest_async_storage_internal_tests ${LIBS_INCLUDE_DIR}/tests/GuestAsyncStorageInternal.cpp)
    target_include_directories(guest_async_storage_internal_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_async_storage_internal_tests PRIVATE libSceAsyncStorageInternal libc)
    configure_windows_unwind(guest_async_storage_internal_tests)
    add_test(NAME guest_async_storage_internal COMMAND guest_async_storage_internal_tests)
    if(WIN32)
        set_tests_properties(guest_async_storage_internal PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAsyncStorageInternal>")
    endif()
endif()
