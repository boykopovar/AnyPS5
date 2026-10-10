add_library(libkernel SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Apr/src/Apr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/Equeue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/src/EventAccessors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/DirectoryDescriptor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/NativeStat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Terminal.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Open.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/OpenFileLimit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Stdio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libc/src/Rename.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Semaphore/src/Semaphore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SyncOnAddress/src/SyncOnAddress.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/DirectMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/PosixMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPoolExport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Mutex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Cond.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Attr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Tsd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Rwlock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Sem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Attr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Barrier.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Cond.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Mutex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Rwlock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Once.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Tsd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Sem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/Socket.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/Udp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/AddressConversion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/EventFlag/src/EventFlag.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Module/src/Module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Module/src/DynamicLoader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Module/src/ModuleInfo.cpp
        ${LIBS_INCLUDE_DIR}/nid/src/NidCompute.cpp
        ${LIBS_INCLUDE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Aio/src/Aio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Coredump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Exception.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Sanitizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Rtld.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Sandbox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/SystemConfiguration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Signals.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/SignalSets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Notification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Time/Time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Time/TimedWait.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AppMetadata/src/AppMetadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AppMetadata/src/ParamJsonParser.cpp
)

target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR})
target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR}/nid/include)
target_link_libraries(libkernel PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(libkernel PRIVATE libc)
if(WIN32)
    target_link_libraries(libkernel PRIVATE ws2_32)
    target_compile_definitions(libkernel PRIVATE _WIN32_WINNT=0x0602)
endif()
configure_windows_unwind(libkernel)

if(BUILD_TESTING)
    foreach(downloadDataCase IN ITEMS declared:256 none:0 invalid:256)
        string(REPLACE ":" ";" downloadDataCase ${downloadDataCase})
        list(GET downloadDataCase 0 downloadDataName)
        list(GET downloadDataCase 1 downloadDataSize)
        set(downloadDataRun ${CMAKE_CURRENT_BINARY_DIR}/download_data_mount_${downloadDataName})
        if(downloadDataName STREQUAL "invalid")
            file(WRITE ${downloadDataRun}/app0/sce_sys/param.json "{\"downloadDataSize\":${downloadDataSize}}")
        else()
            file(WRITE ${downloadDataRun}/app0/sce_sys/param.json
                "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}},\"downloadDataSize\":${downloadDataSize},"
                "\"userDefinedParam1\":0,\"userDefinedParam2\":0,\"userDefinedParam3\":0,\"userDefinedParam4\":0}")
        endif()
        if(downloadDataName STREQUAL "declared")
            anyps5_add_library_test(download_data_mount_tests
                NAME download_data_mount_declared
                SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestDownloadDataMount.cpp
                LINK libkernel libc
                ARGS declared
                PROPERTIES WORKING_DIRECTORY ${downloadDataRun}
            )
        else()
            add_test(NAME download_data_mount_${downloadDataName} COMMAND download_data_mount_tests ${downloadDataName}
                WORKING_DIRECTORY ${downloadDataRun})
            anyps5_set_test_properties(download_data_mount_${downloadDataName} integration "" SKIP_RETURN_CODE 77)
            if(WIN32)
                set_tests_properties(download_data_mount_${downloadDataName} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
            endif()
        endif()
    endforeach()
    add_test_executable(param_json_parser_tests ${LIBS_INCLUDE_DIR}/tests/ParamJsonParser.cpp)
    target_include_directories(param_json_parser_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(param_json_parser_tests PRIVATE libkernel libc)
    configure_windows_unwind(param_json_parser_tests)
    add_test(NAME param_json_parser COMMAND param_json_parser_tests)
    if(WIN32)
        set_tests_properties(param_json_parser PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_posix_sem_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPosixSem.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_process_time_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestProcessTime.cpp
        LINK libkernel libc
    )
    add_library(guest_module_fixture SHARED ${LIBS_INCLUDE_DIR}/tests/GuestModuleFixture.cpp)
    target_include_directories(guest_module_fixture PRIVATE ${LIBS_INCLUDE_DIR})
    add_custom_command(TARGET guest_module_fixture POST_BUILD
        COMMAND $<TARGET_FILE:nid_patcher> guest_module_fixture $<TARGET_FILE:guest_module_fixture>
        VERBATIM)
    add_dependencies(guest_module_fixture nid_patcher)
    configure_windows_unwind(guest_module_fixture)
    set_target_properties(guest_module_fixture PROPERTIES BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "$ORIGIN")
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND AND NOT WIN32)
        target_link_options(guest_module_fixture PRIVATE -Wl,--no-as-needed)
        anyps5_add_python_test(nid_patcher_dynamic_strings LABEL e2e
            SCRIPT ${LIBS_INCLUDE_DIR}/nid/tests/e2e/test_dynamic_strings.py
            ARGS $<TARGET_FILE:guest_module_fixture>)
        anyps5_add_python_test(nid_patcher_symbol_versions LABEL e2e
            SCRIPT ${LIBS_INCLUDE_DIR}/nid/tests/e2e/test_symbol_versions.py
            ARGS $<TARGET_FILE:nid_patcher>)
    endif()
    add_test_executable(guest_dynamic_loader_tests ${LIBS_INCLUDE_DIR}/tests/GuestDynamicLoader.cpp)
    target_include_directories(guest_dynamic_loader_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_dynamic_loader_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_dynamic_loader_tests)
    add_dependencies(guest_dynamic_loader_tests guest_module_fixture)
    if(NOT WIN32)
        set_target_properties(guest_dynamic_loader_tests PROPERTIES ENABLE_EXPORTS ON)
    endif()
    add_test(NAME guest_dynamic_loader COMMAND guest_dynamic_loader_tests $<TARGET_FILE_NAME:guest_module_fixture>)
    set_tests_properties(guest_dynamic_loader PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:guest_module_fixture>")
    if(WIN32)
        set_tests_properties(guest_dynamic_loader PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_module_info_tests ${LIBS_INCLUDE_DIR}/tests/GuestModuleInfo.cpp)
    target_include_directories(guest_module_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_module_info_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_module_info_tests)
    add_dependencies(guest_module_info_tests guest_module_fixture)
    add_test(NAME guest_module_info COMMAND guest_module_info_tests $<TARGET_FILE_NAME:guest_module_fixture>)
    set_tests_properties(guest_module_info PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:guest_module_fixture>")
    if(WIN32)
        set_tests_properties(guest_module_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_file_timestamp_tests
        NAME guest_file_timestamps
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestFileTimestamps.cpp
        LINK libkernel libc
        RUNTIME libc
    )
    anyps5_add_library_test(guest_filesystem_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestFilesystem.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_terminal_tests ${LIBS_INCLUDE_DIR}/tests/GuestTerminal.cpp)
    target_include_directories(guest_terminal_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_terminal_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_terminal_tests)
    add_test(NAME guest_terminal COMMAND guest_terminal_tests)
    set_tests_properties(guest_terminal PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_terminal PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_kernel_sync_tests ${LIBS_INCLUDE_DIR}/tests/GuestKernelSync.cpp)
    target_include_directories(guest_kernel_sync_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_kernel_sync_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_kernel_sync_tests)
    add_test(NAME guest_kernel_sync COMMAND guest_kernel_sync_tests)
    if(WIN32)
        set_tests_properties(guest_kernel_sync PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_kernel_rename_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestKernelRename.cpp
        LINK libkernel libc
    )
    target_compile_definitions(guest_kernel_rename_tests PRIVATE KERNEL_PRX_PATH="$<TARGET_FILE:libkernel>")
    anyps5_add_library_test(guest_aio_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAio.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_once_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestOnce.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_pthread_barrier_tests ${LIBS_INCLUDE_DIR}/tests/GuestPthreadBarrier.cpp)
    target_include_directories(guest_pthread_barrier_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_barrier_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_barrier_tests)
    add_test(NAME guest_pthread_barrier COMMAND guest_pthread_barrier_tests)
    set_tests_properties(guest_pthread_barrier PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_barrier PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_kernel_is_stack_tests ${LIBS_INCLUDE_DIR}/tests/GuestKernelIsStack.cpp)
    target_include_directories(guest_kernel_is_stack_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_kernel_is_stack_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_kernel_is_stack_tests)
    add_test(NAME guest_kernel_is_stack COMMAND guest_kernel_is_stack_tests)
    set_tests_properties(guest_kernel_is_stack PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_is_stack PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_thread_self_tests ${LIBS_INCLUDE_DIR}/tests/GuestThreadSelf.cpp)
    target_include_directories(guest_thread_self_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_thread_self_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_thread_self_tests)
    add_test(NAME guest_thread_self COMMAND guest_thread_self_tests)
    set_tests_properties(guest_thread_self PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_thread_self PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_pthread_getname_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPthreadGetname.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_pthread_attr_tests ${LIBS_INCLUDE_DIR}/tests/GuestPthreadAttr.cpp)
    target_include_directories(guest_pthread_attr_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_attr_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_attr_tests)
    add_test(NAME guest_pthread_attr COMMAND guest_pthread_attr_tests)
    set_tests_properties(guest_pthread_attr PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_attr PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_pthread_start_tests ${LIBS_INCLUDE_DIR}/tests/PthreadStart.cpp)
    target_include_directories(guest_pthread_start_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_start_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_start_tests)
    add_test(NAME guest_pthread_start COMMAND guest_pthread_start_tests)
    set_tests_properties(guest_pthread_start PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_start PROPERTIES TIMEOUT 60 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_cond_timedwait_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestCondTimedwait.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_cond_recursive_mutex_tests ${LIBS_INCLUDE_DIR}/tests/GuestCondRecursiveMutex.cpp)
    target_include_directories(guest_cond_recursive_mutex_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_cond_recursive_mutex_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_cond_recursive_mutex_tests)
    add_test(NAME guest_cond_recursive_mutex COMMAND guest_cond_recursive_mutex_tests)
    set_tests_properties(guest_cond_recursive_mutex PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_cond_recursive_mutex PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_cpu_time_clocks_tests ${LIBS_INCLUDE_DIR}/tests/GuestCpuTimeClocks.cpp)
    target_include_directories(guest_cpu_time_clocks_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_cpu_time_clocks_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_cpu_time_clocks_tests)
    add_test(NAME guest_cpu_time_clocks COMMAND guest_cpu_time_clocks_tests)
    set_tests_properties(guest_cpu_time_clocks PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_cpu_time_clocks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_freebsd_clocks_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestFreebsdClocks.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_thread_cpu_clock_tests ${LIBS_INCLUDE_DIR}/tests/GuestThreadCpuClock.cpp)
    target_include_directories(guest_thread_cpu_clock_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_thread_cpu_clock_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_thread_cpu_clock_tests)
    add_test(NAME guest_thread_cpu_clock COMMAND guest_thread_cpu_clock_tests)
    set_tests_properties(guest_thread_cpu_clock PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_thread_cpu_clock PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_nanosleep_tests ${LIBS_INCLUDE_DIR}/tests/GuestNanosleep.cpp)
    target_include_directories(guest_nanosleep_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_nanosleep_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_nanosleep_tests)
    add_test(NAME guest_nanosleep COMMAND guest_nanosleep_tests)
    set_tests_properties(guest_nanosleep PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_nanosleep PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_pthread_sem_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPthreadSem.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_semaphore_tests ${LIBS_INCLUDE_DIR}/tests/GuestSemaphore.cpp)
    target_include_directories(guest_semaphore_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_semaphore_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_semaphore_tests)
    add_test(NAME guest_semaphore COMMAND guest_semaphore_tests)
    set_tests_properties(guest_semaphore PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_semaphore PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_pthread_rwlock_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPthreadRwlock.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_pthread_key_tests ${LIBS_INCLUDE_DIR}/tests/GuestPthreadKey.cpp)
    target_include_directories(guest_pthread_key_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_key_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_key_tests)
    add_test(NAME guest_pthread_key COMMAND guest_pthread_key_tests)
    set_tests_properties(guest_pthread_key PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_signal_tests ${LIBS_INCLUDE_DIR}/tests/GuestSignals.cpp)
    target_include_directories(guest_signal_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_signal_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_signal_tests)
    add_test(NAME guest_signals COMMAND guest_signal_tests)
    if(WIN32)
        set_tests_properties(guest_signals PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_signal_set_tests ${LIBS_INCLUDE_DIR}/tests/GuestSignalSets.cpp)
    target_include_directories(guest_signal_set_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_signal_set_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_signal_set_tests)
    add_test(NAME guest_signal_sets COMMAND guest_signal_set_tests)
    set_tests_properties(guest_signal_sets PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_signal_sets PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_sigaltstack_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSigaltstack.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_exception_handler_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestExceptionHandler.cpp
        LINK libkernel libc
    )
    if(WIN32)
        anyps5_add_library_test(guest_raise_exception_tests
            TIMEOUT 30
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestRaiseException.cpp
            LINK libkernel libc
        )
        anyps5_add_library_test(guest_raise_nested_sleep_tests
            TIMEOUT 30
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestRaiseNestedSleep.cpp
            LINK libkernel libc
        )
    endif()
    if(WIN32)
        add_library(guest_unwind_module_info_fixture SHARED ${LIBS_INCLUDE_DIR}/tests/GuestUnwindModuleInfoFixture.cpp)
        target_include_directories(guest_unwind_module_info_fixture PRIVATE ${LIBS_INCLUDE_DIR})
        target_compile_options(guest_unwind_module_info_fixture PRIVATE -fno-exceptions)
        anyps5_add_library_test(guest_unwind_module_info_tests
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUnwindModuleInfo.cpp
            LINK libkernel libc guest_unwind_module_info_fixture
        )
        set_tests_properties(guest_unwind_module_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>;PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_unwind_module_info_fixture>")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_library(guest_unwind_module_info_fixture SHARED ${LIBS_INCLUDE_DIR}/tests/GuestModuleFixture.cpp)
        target_include_directories(guest_unwind_module_info_fixture PRIVATE ${LIBS_INCLUDE_DIR})
        set_target_properties(guest_unwind_module_info_fixture PROPERTIES PREFIX "" OUTPUT_NAME guest_unwind_module_info_fixture.prx SUFFIX ".guest.prx")
        add_library(guest_unwind_no_header_fixture SHARED ${LIBS_INCLUDE_DIR}/tests/GuestModuleFixture.cpp)
        target_include_directories(guest_unwind_no_header_fixture PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_options(guest_unwind_no_header_fixture PRIVATE -Wl,--no-eh-frame-hdr)
        set_target_properties(guest_unwind_no_header_fixture PROPERTIES PREFIX "" OUTPUT_NAME guest_unwind_no_header_fixture.prx SUFFIX ".guest.prx")
        add_test_executable(guest_unwind_module_info_tests ${LIBS_INCLUDE_DIR}/tests/GuestUnwindModuleInfoLinux.cpp)
        target_include_directories(guest_unwind_module_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_unwind_module_info_tests PRIVATE libkernel libc)
        add_dependencies(guest_unwind_module_info_tests guest_unwind_module_info_fixture guest_unwind_no_header_fixture)
        add_test(NAME guest_unwind_module_info COMMAND guest_unwind_module_info_tests guest_unwind_module_info_fixture.prx guest_unwind_no_header_fixture.prx)
        set_tests_properties(guest_unwind_module_info PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:guest_unwind_module_info_fixture>")
        anyps5_add_library_test(guest_unwind_module_info_failure_tests
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUnwindModuleInfoFailureLinux.cpp
            LINK libkernel libc
        )
        set_target_properties(guest_unwind_module_info_failure_tests PROPERTIES ENABLE_EXPORTS ON)
    endif()
    anyps5_add_library_test(guest_sync_on_address_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSyncOnAddress.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_available_cpumask_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAvailableCpumask.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_current_cpu_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestCurrentCpu.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_getrusage_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestGetrusage.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_kernel_error_tests
        NAME guest_kernel_errors
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestKernelErrors.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_getrlimit_tests ${LIBS_INCLUDE_DIR}/tests/GuestGetrlimit.cpp)
    target_include_directories(guest_getrlimit_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_getrlimit_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_getrlimit_tests)
    add_test(NAME guest_getrlimit COMMAND guest_getrlimit_tests)
    set_tests_properties(guest_getrlimit PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_getrlimit PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_equeue_event_tests
        NAME guest_equeue_events
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestEqueueEvents.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_system_configuration_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemConfiguration.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_system_command_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemCommand.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_udp_tests ${LIBS_INCLUDE_DIR}/tests/GuestUdp.cpp)
    target_include_directories(guest_udp_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_udp_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_udp_tests)
    add_test(NAME guest_udp COMMAND guest_udp_tests)
    set_tests_properties(guest_udp PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_udp PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_socket_read_write_tests ${LIBS_INCLUDE_DIR}/tests/GuestSocketReadWrite.cpp)
    target_include_directories(guest_socket_read_write_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_socket_read_write_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_socket_read_write_tests)
    add_test(NAME guest_socket_read_write COMMAND guest_socket_read_write_tests)
    set_tests_properties(guest_socket_read_write PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_socket_read_write PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_poll_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPoll.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_tcp_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestTcp.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_socket_pair_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSocketPair.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_elf_phdr_match_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestElfPhdrMatch.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_credentials_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestCredentials.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_address_tests
        NAME guest_addresses
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAddressConversion.cpp
        LINK libkernel libc
    )
    anyps5_add_library_test(guest_memory_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestMemory.cpp
        LINK libkernel libc
    )
    add_test_executable(guest_mlock_tests ${LIBS_INCLUDE_DIR}/tests/GuestMlock.cpp)
    target_include_directories(guest_mlock_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_mlock_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_mlock_tests)
    add_test(NAME guest_mlock COMMAND guest_mlock_tests)
    set_tests_properties(guest_mlock PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_mlock PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_msync_tests ${LIBS_INCLUDE_DIR}/tests/GuestMsync.cpp)
    target_include_directories(guest_msync_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_msync_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_msync_tests)
    add_test(NAME guest_msync COMMAND guest_msync_tests)
    set_tests_properties(guest_msync PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_msync PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    if(WIN32)
        anyps5_add_library_test(guest_directory_buffer_tests
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestDirectoryBuffer.cpp
            LINK libkernel libc
        )
    endif()
    if(NOT WIN32)
        anyps5_add_library_test(guest_directory_entry_tests
            NAME guest_directory_entries
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestDirectoryEntries.cpp
            LINK libkernel libc
        )
    else()
        anyps5_add_library_test(guest_directory_close_tests
            TIMEOUT 15
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestDirectoryClose.cpp
            LINK libkernel libc
        )
        if(MINGW)
            target_compile_options(guest_directory_close_tests PRIVATE -fasynchronous-unwind-tables)
        endif()
    endif()
    add_test_executable(guest_file_vector_tests ${LIBS_INCLUDE_DIR}/tests/GuestFileVector.cpp)
    target_include_directories(guest_file_vector_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_file_vector_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_file_vector_tests)
    add_test(NAME guest_file_vector COMMAND guest_file_vector_tests)
    if(WIN32)
        set_tests_properties(guest_file_vector PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    anyps5_add_library_test(guest_sandbox_random_word_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSandboxRandomWord.cpp
        LINK libkernel libc
    )
    if(WIN32)
        anyps5_add_library_test(guest_mutex_raise_exception_tests
            TIMEOUT 20
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestMutexRaiseException.cpp
            LINK libkernel libc
        )
    endif()
endif()
