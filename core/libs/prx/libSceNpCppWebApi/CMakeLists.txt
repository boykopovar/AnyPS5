add_library(libSceNpCppWebApi SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpCppWebApi PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpCppWebApi)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_np_cpp_web_api_string_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpCppWebApiString.cpp)
    target_include_directories(guest_np_cpp_web_api_string_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_cpp_web_api_string_tests PRIVATE libSceNpCppWebApi libc)
    configure_windows_unwind(guest_np_cpp_web_api_string_tests)
    add_test(NAME guest_np_cpp_web_api_string COMMAND guest_np_cpp_web_api_string_tests)
    add_test(NAME guest_np_cpp_web_api_alias_append COMMAND guest_np_cpp_web_api_string_tests alias-append)
    add_test(NAME guest_np_cpp_web_api_alias_string_vector COMMAND guest_np_cpp_web_api_string_tests alias-vector)
    add_test(NAME guest_np_cpp_web_api_alias_intrusive_vector COMMAND guest_np_cpp_web_api_intrusive_ptr_tests alias-vector)
    set_tests_properties(guest_np_cpp_web_api_alias_append guest_np_cpp_web_api_alias_string_vector
        guest_np_cpp_web_api_alias_intrusive_vector PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_np_cpp_web_api_string guest_np_cpp_web_api_alias_append
            guest_np_cpp_web_api_alias_string_vector guest_np_cpp_web_api_alias_intrusive_vector
            PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
