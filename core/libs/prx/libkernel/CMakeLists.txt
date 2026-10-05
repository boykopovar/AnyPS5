add_library(libkernel SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Apr/src/Apr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/Equeue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/src/EventAccessors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/DirectoryDescriptor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/NativeStat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Open.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Stdio.cpp
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
        ${LIBS_INCLUDE_DIR}/nid/src/NidCompute.cpp
        ${LIBS_INCLUDE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Aio/src/Aio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Coredump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Exception.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Sanitizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Rtld.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/SystemConfiguration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Signals.cpp
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

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(param_json_parser_tests ${LIBS_INCLUDE_DIR}/tests/ParamJsonParser.cpp)
    target_include_directories(param_json_parser_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(param_json_parser_tests PRIVATE libkernel libc)
    configure_windows_unwind(param_json_parser_tests)
    add_test(NAME param_json_parser COMMAND param_json_parser_tests)
    if(WIN32)
        set_tests_properties(param_json_parser PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_posix_sem_tests ${LIBS_INCLUDE_DIR}/tests/GuestPosixSem.cpp)
    target_include_directories(guest_posix_sem_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_posix_sem_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_posix_sem_tests)
    add_test(NAME guest_posix_sem_init COMMAND guest_posix_sem_tests init)
    add_test(NAME guest_posix_sem_post COMMAND guest_posix_sem_tests post)
    if(WIN32)
        set_tests_properties(guest_posix_sem_init guest_posix_sem_post PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_library(guest_module_fixture SHARED ${LIBS_INCLUDE_DIR}/tests/GuestModuleFixture.cpp)
    target_include_directories(guest_module_fixture PRIVATE ${LIBS_INCLUDE_DIR})
    add_custom_command(TARGET guest_module_fixture POST_BUILD
        COMMAND $<TARGET_FILE:nid_patcher> guest_module_fixture $<TARGET_FILE:guest_module_fixture>
        VERBATIM)
    add_dependencies(guest_module_fixture nid_patcher)
    add_test_executable(guest_dynamic_loader_tests ${LIBS_INCLUDE_DIR}/tests/GuestDynamicLoader.cpp)
    target_include_directories(guest_dynamic_loader_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_dynamic_loader_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_dynamic_loader_tests)
    add_dependencies(guest_dynamic_loader_tests guest_module_fixture)
    add_test(NAME guest_dynamic_loader COMMAND guest_dynamic_loader_tests $<TARGET_FILE_NAME:guest_module_fixture>)
    set_tests_properties(guest_dynamic_loader PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:guest_module_fixture>")
    if(WIN32)
        set_tests_properties(guest_dynamic_loader PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_filesystem_tests ${LIBS_INCLUDE_DIR}/tests/GuestFilesystem.cpp)
    target_include_directories(guest_filesystem_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_filesystem_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_filesystem_tests)
    add_test(NAME guest_filesystem COMMAND guest_filesystem_tests)
    if(WIN32)
        set_tests_properties(guest_filesystem PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_aio_tests ${LIBS_INCLUDE_DIR}/tests/GuestAio.cpp)
    target_include_directories(guest_aio_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_aio_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_aio_tests)
    add_test(NAME guest_aio COMMAND guest_aio_tests)
    if(WIN32)
        set_tests_properties(guest_aio PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_once_tests ${LIBS_INCLUDE_DIR}/tests/GuestOnce.cpp)
    target_include_directories(guest_once_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_once_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_once_tests)
    add_test(NAME guest_once COMMAND guest_once_tests)
    set_tests_properties(guest_once PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_once PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
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
    add_test_executable(guest_cond_timedwait_tests ${LIBS_INCLUDE_DIR}/tests/GuestCondTimedwait.cpp)
    target_include_directories(guest_cond_timedwait_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_cond_timedwait_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_cond_timedwait_tests)
    add_test(NAME guest_cond_timedwait COMMAND guest_cond_timedwait_tests)
    set_tests_properties(guest_cond_timedwait PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_cond_timedwait PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(guest_freebsd_clocks_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreebsdClocks.cpp)
    target_include_directories(guest_freebsd_clocks_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freebsd_clocks_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_freebsd_clocks_tests)
    add_test(NAME guest_freebsd_clocks COMMAND guest_freebsd_clocks_tests)
    set_tests_properties(guest_freebsd_clocks PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freebsd_clocks PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(guest_pthread_sem_tests ${LIBS_INCLUDE_DIR}/tests/GuestPthreadSem.cpp)
    target_include_directories(guest_pthread_sem_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_sem_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_sem_tests)
    add_test(NAME guest_pthread_sem COMMAND guest_pthread_sem_tests)
    set_tests_properties(guest_pthread_sem PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_sem PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
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
    add_test_executable(guest_pthread_rwlock_tests ${LIBS_INCLUDE_DIR}/tests/GuestPthreadRwlock.cpp)
    target_include_directories(guest_pthread_rwlock_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pthread_rwlock_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_pthread_rwlock_tests)
    add_test(NAME guest_pthread_rwlock COMMAND guest_pthread_rwlock_tests)
    set_tests_properties(guest_pthread_rwlock PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_pthread_rwlock PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
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
    add_test_executable(guest_sync_on_address_tests ${LIBS_INCLUDE_DIR}/tests/GuestSyncOnAddress.cpp)
    target_include_directories(guest_sync_on_address_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_sync_on_address_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_sync_on_address_tests)
    add_test(NAME guest_sync_on_address COMMAND guest_sync_on_address_tests)
    set_tests_properties(guest_sync_on_address PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_sync_on_address PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_kernel_error_tests ${LIBS_INCLUDE_DIR}/tests/GuestKernelErrors.cpp)
    target_include_directories(guest_kernel_error_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_kernel_error_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_kernel_error_tests)
    add_test(NAME guest_kernel_errors COMMAND guest_kernel_error_tests)
    set_tests_properties(guest_kernel_errors PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_errors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_equeue_event_tests ${LIBS_INCLUDE_DIR}/tests/GuestEqueueEvents.cpp)
    target_include_directories(guest_equeue_event_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_equeue_event_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_equeue_event_tests)
    add_test(NAME guest_equeue_events COMMAND guest_equeue_event_tests)
    set_tests_properties(guest_equeue_events PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_equeue_events PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_system_configuration_tests ${LIBS_INCLUDE_DIR}/tests/GuestSystemConfiguration.cpp)
    target_include_directories(guest_system_configuration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_system_configuration_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_system_configuration_tests)
    add_test(NAME guest_system_configuration COMMAND guest_system_configuration_tests)
    if(WIN32)
        set_tests_properties(guest_system_configuration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
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
    add_test_executable(guest_tcp_tests ${LIBS_INCLUDE_DIR}/tests/GuestTcp.cpp)
    target_include_directories(guest_tcp_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_tcp_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_tcp_tests)
    add_test(NAME guest_tcp COMMAND guest_tcp_tests)
    set_tests_properties(guest_tcp PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_tcp PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_elf_phdr_match_tests ${LIBS_INCLUDE_DIR}/tests/GuestElfPhdrMatch.cpp)
    target_include_directories(guest_elf_phdr_match_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_elf_phdr_match_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_elf_phdr_match_tests)
    add_test(NAME guest_elf_phdr_match COMMAND guest_elf_phdr_match_tests)
    set_tests_properties(guest_elf_phdr_match PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_elf_phdr_match PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_address_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressConversion.cpp)
    target_include_directories(guest_address_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_address_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_address_tests)
    add_test(NAME guest_addresses COMMAND guest_address_tests)
    if(WIN32)
        set_tests_properties(guest_addresses PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(guest_memory_tests ${LIBS_INCLUDE_DIR}/tests/GuestMemory.cpp)
    target_include_directories(guest_memory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_memory_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_memory_tests)
    add_test(NAME guest_memory COMMAND guest_memory_tests)
    if(WIN32)
        set_tests_properties(guest_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    if(WIN32)
        add_test_executable(guest_directory_buffer_tests ${LIBS_INCLUDE_DIR}/tests/GuestDirectoryBuffer.cpp)
        target_include_directories(guest_directory_buffer_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_directory_buffer_tests PRIVATE libkernel libc)
        configure_windows_unwind(guest_directory_buffer_tests)
        add_test(NAME guest_directory_buffer COMMAND guest_directory_buffer_tests)
        set_tests_properties(guest_directory_buffer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    if(NOT WIN32)
        add_test_executable(guest_directory_entry_tests ${LIBS_INCLUDE_DIR}/tests/GuestDirectoryEntries.cpp)
        target_include_directories(guest_directory_entry_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_directory_entry_tests PRIVATE libkernel libc)
        add_test(NAME guest_directory_entries COMMAND guest_directory_entry_tests)
    else()
        add_test_executable(guest_directory_close_tests ${LIBS_INCLUDE_DIR}/tests/GuestDirectoryClose.cpp)
        target_include_directories(guest_directory_close_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_directory_close_tests PRIVATE libkernel libc)
        configure_windows_unwind(guest_directory_close_tests)
        add_test(NAME guest_directory_close COMMAND guest_directory_close_tests)
        set_tests_properties(guest_directory_close PROPERTIES TIMEOUT 15 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
        if(MINGW)
            target_compile_options(guest_directory_close_tests PRIVATE -fasynchronous-unwind-tables)
        endif()
    endif()
endif()
