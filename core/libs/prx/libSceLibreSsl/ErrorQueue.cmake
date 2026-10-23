find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(libreSslSource ${PROJECT_SOURCE_DIR}/3rdparty/libressl-openbsd/src/lib/libcrypto)
if(NOT EXISTS ${libreSslSource}/err/err.c)
    message(FATAL_ERROR "Initialize 3rdparty/libressl-openbsd with git submodule update --init")
endif()
file(GLOB_RECURSE libreSslStringSources CONFIGURE_DEPENDS ${libreSslSource}/*.h ${libreSslSource}/*.c)
set(libreSslStrings ${CMAKE_CURRENT_BINARY_DIR}/prx/libSceLibreSsl/ErrorStrings.inc)
add_custom_command(OUTPUT ${libreSslStrings}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/GenerateErrorStrings.py ${libreSslSource} ${libreSslStrings}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/GenerateErrorStrings.py ${libreSslStringSources}
    VERBATIM)
add_custom_target(libressl_error_strings DEPENDS ${libreSslStrings})
add_dependencies(libSceLibreSsl libressl_error_strings)
target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/Error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ErrorQueue.cpp ${libreSslStrings})
target_include_directories(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/prx/libSceLibreSsl)
target_link_libraries(libSceLibreSsl3 PRIVATE libSceLibreSsl)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_error_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslError.cpp)
    target_include_directories(guest_libressl_error_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_error_tests PRIVATE libSceLibreSsl libSceLibreSsl3)
    configure_windows_unwind(guest_libressl_error_tests)
    add_test(NAME guest_libressl_error COMMAND guest_libressl_error_tests)
    add_test_executable(guest_libressl_error_string_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslErrors.cpp)
    target_include_directories(guest_libressl_error_string_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_error_string_tests PRIVATE libSceLibreSsl libSceLibreSsl3)
    configure_windows_unwind(guest_libressl_error_string_tests)
    add_test(NAME guest_libressl_errors COMMAND guest_libressl_error_string_tests)
    add_test(NAME guest_libressl_free_strings COMMAND guest_libressl_error_string_tests free)
    add_test(NAME guest_libressl_free_strings_first COMMAND guest_libressl_error_string_tests free-first)
    set_tests_properties(guest_libressl_error guest_libressl_errors guest_libressl_free_strings
        guest_libressl_free_strings_first PROPERTIES TIMEOUT 15)
    if(WIN32)
        set_tests_properties(guest_libressl_error guest_libressl_errors guest_libressl_free_strings
            guest_libressl_free_strings_first PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()
