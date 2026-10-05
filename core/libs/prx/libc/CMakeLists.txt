target_sources(libc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/General.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/gcc/StackGuard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/windows/CrashReport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/linux/DlIteratePhdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Strings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AllocatingStrings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WideConversion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Multibyte.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StringExtensions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Math.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FileAndHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StdioExtras.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StandardStreams.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestAllocations.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestArena.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestWriteWatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HeapDiagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Mspace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationProcessParameters.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/JumpBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Formatting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FormattingWide.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LocaleSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Environment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Directory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Filesystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Errors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CxxAbiSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/exception/Exports.cpp
)

target_include_directories(libc PRIVATE
        ${LIBS_INCLUDE_DIR}
)

if(BUILD_TESTING)
    add_test_executable(file_position_tests ${LIBS_INCLUDE_DIR}/tests/FilePosition.cpp)
    target_include_directories(file_position_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(file_position_tests PRIVATE libc)
    configure_windows_unwind(file_position_tests)
    add_test(NAME file_position COMMAND file_position_tests)
    if(WIN32)
        set_tests_properties(file_position PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_wide_formatting_tests ${LIBS_INCLUDE_DIR}/tests/GuestWideFormatting.cpp)
    target_include_directories(guest_wide_formatting_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_wide_formatting_tests PRIVATE libc)
    configure_windows_unwind(guest_wide_formatting_tests)
    add_test(NAME guest_wide_formatting COMMAND guest_wide_formatting_tests)
    if(WIN32)
        set_tests_properties(guest_wide_formatting PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_character_tests ${LIBS_INCLUDE_DIR}/tests/GuestCharacter.cpp)
    target_include_directories(guest_character_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_character_tests PRIVATE libc)
    configure_windows_unwind(guest_character_tests)
    add_test(NAME guest_character COMMAND guest_character_tests)
    if(WIN32)
        set_tests_properties(guest_character PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_math_tests ${LIBS_INCLUDE_DIR}/tests/GuestMath.cpp)
    target_include_directories(guest_math_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_math_tests PRIVATE libc)
    configure_windows_unwind(guest_math_tests)
    add_test(NAME guest_math COMMAND guest_math_tests)
    if(WIN32)
        set_tests_properties(guest_math PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_working_directory_tests ${LIBS_INCLUDE_DIR}/tests/GuestWorkingDirectory.cpp)
    target_include_directories(guest_working_directory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_working_directory_tests PRIVATE libc)
    configure_windows_unwind(guest_working_directory_tests)
    add_test(NAME guest_working_directory COMMAND guest_working_directory_tests)
    if(WIN32)
        set_tests_properties(guest_working_directory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_directory_tests ${LIBS_INCLUDE_DIR}/tests/GuestDirectory.cpp)
    target_include_directories(guest_directory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_directory_tests PRIVATE libc)
    configure_windows_unwind(guest_directory_tests)
    add_test(NAME guest_directory COMMAND guest_directory_tests)
    if(WIN32)
        set_tests_properties(guest_directory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_environment_tests ${LIBS_INCLUDE_DIR}/tests/GuestEnvironment.cpp)
    target_include_directories(guest_environment_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_environment_tests PRIVATE libc)
    configure_windows_unwind(guest_environment_tests)
    add_test(NAME guest_environment COMMAND guest_environment_tests)
    if(WIN32)
        set_tests_properties(guest_environment PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_lifecycle_tests ${LIBS_INCLUDE_DIR}/tests/GuestLifecycle.cpp)
    target_include_directories(guest_lifecycle_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_lifecycle_tests PRIVATE libc)
    configure_windows_unwind(guest_lifecycle_tests)
    add_test(NAME guest_lifecycle COMMAND guest_lifecycle_tests)
    if(WIN32)
        set_tests_properties(guest_lifecycle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_reallocalign_tests ${LIBS_INCLUDE_DIR}/tests/GuestReallocalign.cpp)
    target_include_directories(guest_reallocalign_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_reallocalign_tests PRIVATE libc)
    configure_windows_unwind(guest_reallocalign_tests)
    add_test(NAME guest_reallocalign COMMAND guest_reallocalign_tests)
    if(WIN32)
        set_tests_properties(guest_reallocalign PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_standard_stream_tests ${LIBS_INCLUDE_DIR}/tests/GuestStandardStreams.cpp)
    target_include_directories(guest_standard_stream_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_standard_stream_tests PRIVATE libc)
    configure_windows_unwind(guest_standard_stream_tests)
    add_test(NAME guest_standard_streams COMMAND guest_standard_stream_tests)
    if(WIN32)
        set_tests_properties(guest_standard_streams PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_error_tests ${LIBS_INCLUDE_DIR}/tests/GuestErrors.cpp)
    target_include_directories(guest_error_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_error_tests PRIVATE libc)
    configure_windows_unwind(guest_error_tests)
    add_test(NAME guest_errors COMMAND guest_error_tests)
    if(WIN32)
        set_tests_properties(guest_errors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(mspace_tests ${LIBS_INCLUDE_DIR}/tests/Mspace.cpp)
    target_include_directories(mspace_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(mspace_tests PRIVATE libc)
    configure_windows_unwind(mspace_tests)
    add_test(NAME mspace COMMAND mspace_tests)
    if(WIN32)
        set_tests_properties(mspace PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_formatting_tests ${LIBS_INCLUDE_DIR}/tests/Formatting.cpp)
    target_include_directories(guest_formatting_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_formatting_tests PRIVATE libc)
    configure_windows_unwind(guest_formatting_tests)
    if(WIN32)
        add_test(NAME guest_formatting COMMAND guest_formatting_tests)
        set_tests_properties(guest_formatting PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_search_tests ${LIBS_INCLUDE_DIR}/tests/GuestSearch.cpp)
    target_include_directories(guest_search_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_search_tests PRIVATE libc)
    configure_windows_unwind(guest_search_tests)
    add_test(NAME guest_search COMMAND guest_search_tests)
    if(WIN32)
        set_tests_properties(guest_search PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_string_tests ${LIBS_INCLUDE_DIR}/tests/GuestStrings.cpp)
    target_include_directories(guest_string_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_string_tests PRIVATE libc libScePosixForWebKit)
    configure_windows_unwind(guest_string_tests)
    add_test(NAME guest_strings COMMAND guest_string_tests)
    if(WIN32)
        set_tests_properties(guest_strings PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_wcsrtombs_s_tests ${LIBS_INCLUDE_DIR}/tests/GuestWcsrtombsS.cpp)
    target_include_directories(guest_wcsrtombs_s_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_wcsrtombs_s_tests PRIVATE libc)
    configure_windows_unwind(guest_wcsrtombs_s_tests)
    add_test(NAME guest_wcsrtombs_s COMMAND guest_wcsrtombs_s_tests)
    if(WIN32)
        set_tests_properties(guest_wcsrtombs_s PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(UNIX)
        add_test_executable(exception_runtime_tests ${LIBS_INCLUDE_DIR}/tests/ExceptionRuntime.cpp)
        target_include_directories(exception_runtime_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(exception_runtime_tests PRIVATE libc)
        add_test(NAME exception_runtime COMMAND exception_runtime_tests)
        enable_language(C)
        add_test_executable(exception_c_cleanup_tests ${LIBS_INCLUDE_DIR}/tests/ExceptionCCleanup.cpp ${LIBS_INCLUDE_DIR}/tests/ExceptionCCleanup.c)
        set_source_files_properties(${LIBS_INCLUDE_DIR}/tests/ExceptionCCleanup.c PROPERTIES COMPILE_OPTIONS -fexceptions)
        target_link_libraries(exception_c_cleanup_tests PRIVATE libc)
        add_test(NAME exception_c_cleanup COMMAND exception_c_cleanup_tests)
    endif()
endif()

add_test_executable(application_heap_tests EXCLUDE_FROM_ALL ${LIBS_INCLUDE_DIR}/tests/ApplicationHeap.cpp)
target_include_directories(application_heap_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(application_heap_tests PRIVATE libc)
configure_windows_unwind(application_heap_tests)
set_target_properties(application_heap_tests PROPERTIES CXX_EXTENSIONS OFF)
if(BUILD_TESTING)
    set_target_properties(application_heap_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME application_heap_replacement COMMAND application_heap_tests)
    add_test(NAME application_heap_invalid_metadata COMMAND application_heap_tests invalid)
    add_test(NAME application_heap_default COMMAND application_heap_tests default)
    if(WIN32)
        set_tests_properties(application_heap_replacement application_heap_invalid_metadata application_heap_default
            PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(guest_locale_tests EXCLUDE_FROM_ALL ${LIBS_INCLUDE_DIR}/tests/GuestLocale.cpp)
target_include_directories(guest_locale_tests PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(guest_locale_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(guest_locale_tests PRIVATE libc)
configure_windows_unwind(guest_locale_tests)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    add_test_executable(windows_exception_tests EXCLUDE_FROM_ALL ${LIBS_INCLUDE_DIR}/tests/WindowsExceptions.cpp)
    set_target_properties(windows_exception_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(windows_exception_tests PRIVATE libc)
    configure_windows_unwind(windows_exception_tests)
endif()
