target_sources(libc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/General.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/gcc/StackGuard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/windows/CrashReport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/linux/DlIteratePhdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/macos/GuestImages.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Rename.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Errors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Syslog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CxxAbiSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/exception/Exports.cpp
)

target_include_directories(libc PRIVATE
        ${LIBS_INCLUDE_DIR}
)

if(WIN32)
    target_link_libraries(libc PRIVATE advapi32)
endif()

function(libc_add_test_mode name target timeout)
    add_test(NAME ${name} COMMAND ${target} ${ARGN})
    anyps5_set_test_properties(${name} integration "${timeout}" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endfunction()

if(BUILD_TESTING)
    set(libcTests ${LIBS_INCLUDE_DIR}/tests/integration)
    foreach(libcTest IN ITEMS
            guest_time_conversion:GuestTimeConversion:20
            guest_scan_count:GuestScanCount:
            file_position:FilePosition:
            guest_wide_formatting:GuestWideFormatting:
            guest_wide_strings:GuestWideStrings:
            guest_character:GuestCharacter:
            guest_math:GuestMath:
            guest_execute_once:GuestExecuteOnce:20
            guest_std_execute_once:GuestStdExecuteOnce:20
            guest_fiopen:GuestFiopen:20
            guest_working_directory:GuestWorkingDirectory:
            guest_directory:GuestDirectory:
            guest_lifecycle:GuestLifecycle:
            guest_thread_callback_abi:GuestThreadCallbackAbi:20
            guest_reallocalign:GuestReallocalign:
            guest_standard_streams:GuestStandardStreams:
            file_streams:FileStreams:20
            guest_errors:GuestErrors:
            guest_syslog:GuestSyslog:
            mspace:Mspace:
            guest_formatting:Formatting:
            guest_new_handler:GuestNewHandler:
            guest_search:GuestSearch:
            guest_demangle:GuestDemangle:
            guest_wcsrtombs:GuestWcsrtombs:
            guest_wcsrtombs_s:GuestWcsrtombsS:)
        string(REPLACE ":" ";" libcTestFields "${libcTest}")
        list(GET libcTestFields 0 libcTestName)
        list(GET libcTestFields 1 libcTestSource)
        list(GET libcTestFields 2 libcTestTimeout)
        set(libcTestTimeoutArguments)
        if(libcTestTimeout)
            set(libcTestTimeoutArguments TIMEOUT ${libcTestTimeout})
        endif()
        anyps5_add_library_test(${libcTestName}_tests
            ${libcTestTimeoutArguments}
            SOURCES ${libcTests}/${libcTestSource}.cpp
            LINK libc
        )
    endforeach()
    anyps5_add_library_test(guest_environment_tests
        CUSTOM_MAIN
        SOURCES ${libcTests}/GuestEnvironment.cpp
        LINK libc
    )
    anyps5_add_library_test(guest_path_case_tests
        SOURCES ${libcTests}/GuestPathCase.cpp
        LINK libc libkernel
    )
    anyps5_add_library_test(guest_access_tests
        SOURCES ${libcTests}/GuestAccess.cpp
        LINK libc $<$<BOOL:${WIN32}>:advapi32>
    )
    anyps5_add_library_test(guest_strings_tests
        SOURCES ${libcTests}/GuestStrings.cpp
        LINK libc libScePosixForWebKit
    )
    if(UNIX)
        anyps5_add_library_test(exception_runtime_tests
            SOURCES ${libcTests}/ExceptionRuntime.cpp
            LINK libc
        )
        anyps5_add_library_test(exception_personality_tests
            SOURCES ${libcTests}/ExceptionPersonality.cpp
            LINK libc
        )
        anyps5_add_library_test(guest_exit_order_tests
            SOURCES ${libcTests}/GuestExitOrder.cpp
            LINK libc
        )
        if(NOT APPLE)
            enable_language(C)
            set_source_files_properties(${LIBS_INCLUDE_DIR}/tests/ExceptionCCleanup.c PROPERTIES COMPILE_OPTIONS -fexceptions)
            anyps5_add_library_test(exception_c_cleanup_tests
                SOURCES ${libcTests}/ExceptionCCleanup.cpp ${LIBS_INCLUDE_DIR}/tests/ExceptionCCleanup.c
                LINK libc
            )
            anyps5_add_library_test(guest_image_anonymous_tests
                SOURCES ${libcTests}/GuestImageAnonymous.cpp
                LINK libc
            )
        endif()
    endif()

    anyps5_add_library_test(guest_collate_tests
        SOURCES ${libcTests}/GuestCollate.cpp
        LINK libc
    )
    set_target_properties(guest_collate_tests PROPERTIES CXX_EXTENSIONS OFF)

    anyps5_add_library_test(application_heap_tests
        NAME application_heap_replacement
        SOURCES ${libcTests}/ApplicationHeap.cpp
        LINK libc
    )
    set_target_properties(application_heap_tests PROPERTIES CXX_EXTENSIONS OFF)
    libc_add_test_mode(application_heap_invalid_metadata application_heap_tests "" invalid)
    libc_add_test_mode(application_heap_default application_heap_tests "" default)
    libc_add_test_mode(application_heap_exit_order application_heap_tests "" exit-order)
    anyps5_add_library_test(application_heap_stats_tests
        NAME application_heap_stats
        SOURCES ${libcTests}/ApplicationHeapStats.cpp
        LINK libc
    )
    libc_add_test_mode(application_heap_stats_default application_heap_stats_tests "" default)
    libc_add_test_mode(application_heap_stats_missing application_heap_stats_tests "" missing)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        anyps5_add_library_test(cpu_topology_tests
            SOURCES ${libcTests}/CpuTopology.cpp
        )
    endif()

    if(WIN32)
        anyps5_add_library_test(guest_heap_rounding_tests
            CUSTOM_MAIN
            NAME guest_heap_rounding_overflow
            SOURCES ${libcTests}/GuestHeapRounding.cpp
            LINK libc
        )
        set_target_properties(guest_heap_rounding_tests PROPERTIES CXX_EXTENSIONS OFF)
    endif()

    anyps5_add_library_test(guest_locale_tests
        SOURCES ${libcTests}/GuestLocale.cpp
        LINK libc
    )
    set_target_properties(guest_locale_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MINGW)
        enable_language(ASM)
        anyps5_add_library_test(guest_message_destructor_abi_tests
            NAME guest_message_destructor_control
            TIMEOUT 10
            SOURCES ${libcTests}/GuestMessageDestructorAbi.cpp ${libcTests}/GuestMessageDestructorAbi.S
            LINK libc
            ARGS control
        )
        foreach(mode IN ITEMS decoy zero release regex-decoy regex-zero
                unshared-decoy unshared-zero regex-unshared-decoy regex-unshared-zero)
            libc_add_test_mode(guest_message_destructor_${mode} guest_message_destructor_abi_tests 10 ${mode})
        endforeach()
        foreach(kind IN ITEMS message plain)
            foreach(operation IN ITEMS what destroy delete)
                foreach(register IN ITEMS decoy zero)
                    set(mode "vtable-${kind}-${operation}-${register}")
                    libc_add_test_mode(guest_exception_${mode} guest_message_destructor_abi_tests 10 ${mode})
                endforeach()
            endforeach()
        endforeach()
    endif()
endif()

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    if(BUILD_TESTING)
        anyps5_add_library_test(windows_exception_tests
            NAME windows_exceptions
            SOURCES ${libcTests}/WindowsExceptions.cpp
            LINK libc
        )
        set_target_properties(windows_exception_tests PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()
