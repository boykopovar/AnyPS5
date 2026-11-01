set(ANYPS5_LIBS_INCLUDE_DIR ${LIBS_INCLUDE_DIR})
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/compiler-rt/lib/asan_anyps5 ${CMAKE_CURRENT_BINARY_DIR}/compiler-rt-anyps5 EXCLUDE_FROM_ALL)
add_library(libSceAddressSanitizer SHARED EXCLUDE_FROM_ALL Export.cpp)
target_link_libraries(libSceAddressSanitizer PRIVATE compiler_rt_anyps5)
target_include_directories(libSceAddressSanitizer PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/compiler-rt/lib)
target_include_directories(libSceAddressSanitizer PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAddressSanitizer)

if(BUILD_TESTING)
    add_test_executable(guest_addresssanitizer_shadow_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerShadow.cpp)
    target_include_directories(guest_addresssanitizer_shadow_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_addresssanitizer_shadow_tests PRIVATE libSceAddressSanitizer libc)
    configure_windows_unwind(guest_addresssanitizer_shadow_tests)
    add_test(NAME guest_addresssanitizer_shadow COMMAND guest_addresssanitizer_shadow_tests)
    set_tests_properties(guest_addresssanitizer_shadow PROPERTIES TIMEOUT 20)
    add_test_executable(guest_addresssanitizer_runtime_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerRuntime.cpp)
    target_include_directories(guest_addresssanitizer_runtime_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_addresssanitizer_runtime_tests PRIVATE libSceAddressSanitizer libc)
    configure_windows_unwind(guest_addresssanitizer_runtime_tests)
    add_test(NAME guest_addresssanitizer_fake_stack_option COMMAND guest_addresssanitizer_runtime_tests fake_stack_option)
    set_tests_properties(guest_addresssanitizer_fake_stack_option PROPERTIES TIMEOUT 20)
    add_test(NAME guest_addresssanitizer_vptr_cache_data COMMAND guest_addresssanitizer_runtime_tests vptr_cache_data)
    set_tests_properties(guest_addresssanitizer_vptr_cache_data PROPERTIES TIMEOUT 20)
    add_test(NAME guest_addresssanitizer_globals_invalid_array COMMAND guest_addresssanitizer_runtime_tests globals_invalid_array)
    set_tests_properties(guest_addresssanitizer_globals_invalid_array PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_addresssanitizer_fake_stack_option PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        set_tests_properties(guest_addresssanitizer_vptr_cache_data PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        set_tests_properties(guest_addresssanitizer_globals_invalid_array PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME guest_addresssanitizer_legacy_ubsan COMMAND ${Python3_EXECUTABLE}
            ${LIBS_INCLUDE_DIR}/tests/test_addresssanitizer_legacy.py $<TARGET_FILE:guest_addresssanitizer_runtime_tests>)
        set_tests_properties(guest_addresssanitizer_legacy_ubsan PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_addresssanitizer_legacy_ubsan PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        endif()
    endif()
    foreach(mode clean contiguous double_ended memory heap wrapper_call_sites heap_aligned fake_stack no_return globals globals_elf fibers locate init sandbox_notify)
        add_test(NAME guest_addresssanitizer_runtime_${mode} COMMAND guest_addresssanitizer_runtime_tests ${mode})
        set_tests_properties(guest_addresssanitizer_runtime_${mode} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_addresssanitizer_runtime_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        endif()
    endforeach()
    add_test(NAME guest_addresssanitizer_runtime_exp_load4 COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=exp_load4 -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 4 at 0x[0-9a-f]+ thread T-1.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_exp_store16_noabort COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=exp_store16_noabort -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.WRITE of size 16 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_exp_loadN COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=exp_loadN -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 16 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_report_exp_store2 COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=report_exp_store2 -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.WRITE of size 2 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_report_exp_load_n COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=report_exp_load_n -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 6 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_memcpy_source COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=memcpy_source -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 16 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_memset_poison COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=memset_poison -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.WRITE of size 16 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_memmove_poison COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=memmove_poison -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.WRITE of size 8 at .*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_memcpy_overlap COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=memcpy_overlap -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: memcpy-param-overlap: memory ranges [[]0x[0-9a-f]+,0x[0-9a-f]+[)] and [[]0x[0-9a-f]+, 0x[0-9a-f]+[)] overlap.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_memset_negative COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=memset_negative -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: negative-size-param: [(]size=-1[)].*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_container_params COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=container_params -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_contiguous_container:.*beg     : 0x[0-9a-f]+.*new_mid : 0x[0-9a-f]+.ERROR: beg is not aligned by 8.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_double_ended_params COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=double_ended_params -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_double_ended_contiguous_container:.*storage_beg        : 0x[0-9a-f]+.*new_container_end  : 0x[0-9a-f]+.ERROR: storage_beg is not aligned by 8.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_arithmetic COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_arithmetic -DSTATUS=0
        "-DPATTERN=test.cpp:10:5: runtime error: signed integer overflow: 2147483647 [+] 1 cannot be represented in type 'int'.SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior test.cpp:10:5.test.cpp:11:6: runtime error: unsigned integer overflow: 0 - 1 cannot be represented in type 'unsigned int'.SUMMARY: .*test.cpp:12:7: runtime error: signed integer overflow: 65536 [*] 65536 cannot be represented in type 'int'.*test.cpp:13:8: runtime error: negation of -2147483648 cannot be represented in type 'int'. cast to an unsigned type to negate this value to itself.*test.cpp:14:9: runtime error: division by zero.*test.cpp:15:10: runtime error: division of -2147483648 by -1 cannot be represented in type 'int'.*test.cpp:16:11: runtime error: shift exponent 40 is too large for 32-bit type 'int'.*test.cpp:17:12: runtime error: shift exponent -1 is negative.*test.cpp:18:13: runtime error: left shift of negative value -1.*test.cpp:19:14: runtime error: left shift of 1073741824 by 2 places cannot be represented in type 'int'.*test.cpp:20:15: runtime error: index 4 out of bounds for type 'int .4.'.*test.cpp:21:16: runtime error: variable length array bound evaluates to non-positive value -3"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_misc COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_misc -DSTATUS=0
        "-DPATTERN=test.cpp:30:3: runtime error: load of null pointer of type 'int'.SUMMARY.*test.cpp:31:4: runtime error: store to misaligned address 0x[0-9a-f]+ for type 'int', which requires 4 byte alignment.*0x[0-9a-f]+: note: pointer points here. 00 00 00  00.*test.cpp:32:5: runtime error: load of address 0x[0-9a-f]+ with insufficient space for an object of type 'int'.*test.cpp:33:6: runtime error: null pointer passed as argument 2, which is declared to never be null.decl.h:5:1: note: nonnull attribute specified here.*test.cpp:34:7: runtime error: null pointer passed as argument 1, which is declared to never be null.decl.h:6:2: note: _Nonnull type annotation specified here.*test.cpp:35:8: runtime error: applying zero offset to null pointer.*test.cpp:36:9: runtime error: applying non-zero offset 16 to null pointer.*test.cpp:37:10: runtime error: applying non-zero offset to non-null pointer 0x000000001000 produced null pointer.*test.cpp:38:11: runtime error: addition of unsigned offset to 0x000000001000 overflowed to 0x000000000800.*test.cpp:39:12: runtime error: load of value 5, which is not a valid value for type 'bool'.*test.cpp:40:13: runtime error: implicit conversion from type 'int' of value 300 [(]32-bit, signed[)] to type 'unsigned char' changed the value to 44 [(]8-bit, unsigned[)].*test.cpp:41:14: runtime error: 1e[+]10 is outside the range of representable values of type 'int'.*test.cpp:42:15: runtime error: passing zero to ctz[(][)], which is not a valid argument.*test.cpp:43:16: runtime error: assumption of 8 byte alignment for pointer of type 'char [*]' failed.test.cpp:44:17: note: alignment assumption was specified here.*0x[0-9a-f]+: note: address is 1 aligned, misalignment offset is 3 bytes.*test.cpp:45:18: runtime error: null pointer returned from function declared to never return null.decl.h:9:2: note: _Nonnull return type annotation specified here.*test.cpp:46:19: runtime error: call to function [(]unknown[)] through pointer to incorrect function type 'void [(][*][)][(]int[)]'.*note: [(]unknown[)] defined here"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_data COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_data -DSTATUS=0
        "-DPATTERN=data kind=signed-integer-overflow file=test.cpp line=50 column=4 memory=[^ ]+ message=Signed integer overflow: 2147483647 [+] 1 cannot be represented in type 'int'"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_dedup COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_dedup -DSTATUS=0
        "-DPATTERN=dedup count=1"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_abort COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_abort -DSTATUS=1
        "-DPATTERN=test.cpp:70:2: runtime error: unsigned integer overflow: 0 - 1 cannot be represented in type 'unsigned int'.SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior test.cpp:70:2.death callback"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_ubsan_unreachable COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=ubsan_unreachable -DSTATUS=1
        "-DPATTERN=test.cpp:71:3: runtime error: execution reached an unreachable program point.SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior test.cpp:71:3.death callback"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_stats COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_stats -DSTATUS=0
        "-DPATTERN=Stats: [0-9]+M malloced [(][0-9]+M for red zones[)] by [0-9]+ calls.Stats: [0-9]+M realloced by [0-9]+ calls.Stats: [0-9]+M freed by [0-9]+ calls.Stats: [0-9]+M really freed by [0-9]+ calls.Stats: [0-9]+M [(][0-9]+M-[0-9]+M[)] mmaped.*mallocs by size class: .*Stats: malloc large: [0-9]+"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_double_free COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_double_free -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting double-free on 0x[0-9a-f]+ in thread T[0-9]+:.0x[0-9a-f]+ is located 0 bytes inside of 10-byte region.*freed by thread T[0-9]+ here:.*previously allocated by thread T[0-9]+ here:.*SUMMARY: AddressSanitizer: double-free.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_bad_free COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_bad_free -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting free on address which was not malloc[(][)]-ed: 0x[0-9a-f]+ in thread T[0-9]+.*SUMMARY: AddressSanitizer: bad-free.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_interior_free COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_interior_free -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting free on address which was not malloc[(][)]-ed: 0x[0-9a-f]+ in thread T[0-9]+.0x[0-9a-f]+ is located 1 bytes inside of 32-byte region.*SUMMARY: AddressSanitizer: bad-free"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_usable_size_unowned COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_usable_size_unowned -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting to call malloc_usable_size[(][)] for pointer which is not owned: 0x[0-9a-f]+.*SUMMARY: AddressSanitizer: bad-malloc_usable_size"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_allocated_size_unowned COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_allocated_size_unowned -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting to call __sanitizer_get_allocated_size[(][)] for pointer which is not owned: 0x[0-9a-f]+.*SUMMARY: AddressSanitizer: bad-__sanitizer_get_allocated_size"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_calloc_overflow COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_calloc_overflow -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: calloc parameters overflow: count [*] size [(]1099511627776 [*] 1099511627776[)] cannot be represented in type size_t [(]thread T[0-9]+[)].HINT: if you don't care about these errors you may set allocator_may_return_null=1.SUMMARY: AddressSanitizer: calloc-overflow"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_aligned_alloc_bad COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_aligned_alloc_bad -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: invalid alignment requested in aligned_alloc: 24, alignment must be a power of two and the requested size 0x30 must be a multiple of alignment [(]thread T[0-9]+[)].*SUMMARY: AddressSanitizer: invalid-aligned-alloc-alignment"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_posix_memalign_bad COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_posix_memalign_bad -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: invalid alignment requested in posix_memalign: 12, alignment must be a power of two and a multiple of sizeof[(]void[*][)] == 8 [(]thread T[0-9]+[)].*SUMMARY: AddressSanitizer: invalid-posix-memalign-alignment"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_memalign_bad COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_memalign_bad -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: invalid allocation alignment: 24, alignment must be a power of two [(]thread T[0-9]+[)].*SUMMARY: AddressSanitizer: invalid-allocation-alignment"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_too_big COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_too_big -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: requested allocation size 0x20000000000 [(]0x[0-9a-f]+ after adjustments for alignment, red zones etc.[)] exceeds maximum supported size of 0x10000000000 [(]thread T[0-9]+[)].*SUMMARY: AddressSanitizer: allocation-size-too-big"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_heap_realloc_freed COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=heap_realloc_freed -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: attempting double-free on 0x[0-9a-f]+ in thread T[0-9]+:.*is located 0 bytes inside of 10-byte region.*SUMMARY: AddressSanitizer: double-free"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_globals_odr_indicator COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=globals_odr_indicator -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: odr-violation [(]0x[0-9a-f]+[)]:.  [[]1[]] size=24 'newer' test.cpp:5:6.  [[]2[]] size=13 'older' test.cpp:5:6.HINT: if you don't care about these errors you may set ASAN_OPTIONS=detect_odr_violation=0.SUMMARY: AddressSanitizer: odr-violation: global 'newer' at test.cpp:5:6.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_globals_odr_poison COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=globals_odr_poison -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: odr-violation [(]0x[0-9a-f]+[)]:.  [[]1[]] size=24 'newer' test.cpp:5:6.  [[]2[]] size=13 'older' test.cpp:5:6.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_globals_describe COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=globals_describe -DSTATUS=0
        "-DPATTERN=0x[0-9a-f]+ is located 2 bytes inside of global variable 'g2' defined in 'test.cpp:5:6' [(]0x[0-9a-f]+[)] of size 5.  'g2' is ascii string 'text'.0x[0-9a-f]+ is located 1 bytes after global variable 'g2' defined in 'test.cpp:5:6' [(]0x[0-9a-f]+[)] of size 5.*Address 0x[0-9a-f]+ is a wild pointer inside of access range of size 0x000000000001"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_describe_heap COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=describe_heap -DSTATUS=0
        "-DPATTERN=0x[0-9a-f]+ is located 3 bytes inside of 10-byte region [[]0x[0-9a-f]+,0x[0-9a-f]+[)].allocated by thread T[0-9]+ here:.    #0 0x[0-9a-f]+.0x[0-9a-f]+ is located 2 bytes after 10-byte region.*0x[0-9a-f]+ is located 4 bytes before 10-byte region.*freed by thread T[0-9]+ here:.    #0 0x[0-9a-f]+.previously allocated by thread T[0-9]+ here:.*Address 0x000000001000 is a wild pointer inside of access range of size 0x000000000001[.].Address 0x010000000000 is located in the low shadow area"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_fiber_unstarted COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=fiber_unstarted -DSTATUS=1
        "-DPATTERN=ERROR: finishing a fiber switch that has not started"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_fiber_double_start COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=fiber_double_start -DSTATUS=1
        "-DPATTERN=ERROR: starting fiber switch while in fiber switch"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_report_path COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=report_path -DSTATUS=0
        "-DPATTERN=report path ok"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_report_stdout COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=report_stdout -DSTATUS=0
        "-DPATTERN=Address 0x000000003000 is a wild pointer inside of access range of size 0x000000000001"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_report_descriptor COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=report_descriptor -DSTATUS=0
        "-DPATTERN=Address 0x000000004000 is a wild pointer inside of access range of size 0x000000000001"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_stack_trace COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=stack_trace -DSTATUS=0
        "-DPATTERN=    #0 0x[0-9a-f]+  [(].*guest_addresssanitizer_runtime_tests.*[+]0x[0-9a-f]+[)]"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_symbolize COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=symbolize -DSTATUS=0
        "-DPATTERN=symbolized=[(]guest_addresssanitizer_runtime_tests[^+]*[+]0x[0-9a-f]+[)].literal=[%][|]0[|][|][<]null[>][|][|]0x0[|]0[|]0.short=0x[0-9a-f][0-9a-f].global=<>"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_symbolize_bad COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=symbolize_bad -DSTATUS=1
        "-DPATTERN=Unsupported specifier in stack frame format: z"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_coverage COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=coverage -DSTATUS=0
        "-DPATTERN=SanitizerCoverage: ./guest_addresssanitizer_runtime_tests[^ ]*[.][0-9]+[.]sancov: 2 PCs written"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    add_test(NAME guest_addresssanitizer_runtime_fatal_control COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_runtime_tests> -DMODE=fatal_control -DSTATUS=1
        "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 4 at 0x[0-9a-f]+ thread T-1.*ABORTING"
        -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
    set_tests_properties(guest_addresssanitizer_runtime_fatal_control PROPERTIES WILL_FAIL TRUE)
    foreach(mode exp_load4 exp_store16_noabort exp_loadN report_exp_store2 report_exp_load_n memcpy_source memset_poison memmove_poison memcpy_overlap memset_negative container_params double_ended_params ubsan_arithmetic ubsan_misc ubsan_data ubsan_dedup ubsan_abort ubsan_unreachable heap_stats heap_double_free heap_bad_free heap_interior_free heap_usable_size_unowned heap_allocated_size_unowned heap_calloc_overflow heap_aligned_alloc_bad heap_posix_memalign_bad heap_memalign_bad heap_too_big heap_realloc_freed globals_odr_indicator globals_odr_poison globals_describe describe_heap fiber_unstarted fiber_double_start report_path report_stdout report_descriptor stack_trace symbolize symbolize_bad coverage fatal_control)
        set_tests_properties(guest_addresssanitizer_runtime_${mode} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_addresssanitizer_runtime_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        endif()
    endforeach()
    if(WIN32)
        add_test_executable(guest_addresssanitizer_backing_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerBacking.cpp)
        target_include_directories(guest_addresssanitizer_backing_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_addresssanitizer_backing_tests PRIVATE libSceAddressSanitizer libc)
        configure_windows_unwind(guest_addresssanitizer_backing_tests)
        add_test(NAME guest_addresssanitizer_backing_layout COMMAND guest_addresssanitizer_backing_tests)
        set_tests_properties(guest_addresssanitizer_backing_layout PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        add_test_executable(guest_addresssanitizer_mapping_tests ${LIBS_INCLUDE_DIR}/tests/GuestAddressSanitizerMapping.cpp)
        target_include_directories(guest_addresssanitizer_mapping_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_addresssanitizer_mapping_tests PRIVATE libSceAddressSanitizer libc)
        configure_windows_unwind(guest_addresssanitizer_mapping_tests)
        add_test(NAME guest_addresssanitizer_mapping COMMAND guest_addresssanitizer_mapping_tests)
        add_test(NAME guest_addresssanitizer_mapping_collision COMMAND guest_addresssanitizer_mapping_tests collision)
        set_tests_properties(guest_addresssanitizer_mapping guest_addresssanitizer_mapping_collision PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        add_test(NAME guest_addresssanitizer_report_load4 COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=load4 -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x[0-9a-f]+ bp 0x[0-9a-f]+ sp 0x[0-9a-f]+.READ of size 4 at 0x[0-9a-f]+ thread T-1.Shadow bytes around the buggy address:.*=>0x[0-9a-f]+:.*f7.*Shadow byte legend.*callback present=1 type=0 size=4 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        add_test(NAME guest_addresssanitizer_report_load16 COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=load16 -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address .*READ of size 16 at .*callback present=1 type=0 size=16 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        add_test(NAME guest_addresssanitizer_report_storeN COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=storeN -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address .*WRITE of size 16 at .*callback present=1 type=1 size=16 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        add_test(NAME guest_addresssanitizer_report_unaligned COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=unaligned -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address .*WRITE of size 4 at .*callback present=1 type=1 size=4 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        add_test(NAME guest_addresssanitizer_report_partial COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=partial -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address .*READ of size 1 at .*callback present=1 type=0 size=1 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        add_test(NAME guest_addresssanitizer_report_report_error COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:guest_addresssanitizer_mapping_tests> -DMODE=report -DARGUMENT=report_error -DSTATUS=1
            "-DPATTERN=ERROR: AddressSanitizer: use-after-poison on address 0x[0-9a-f]+ at pc 0x000000001000 bp 0x000000002000 sp 0x000000003000.WRITE of size 2 at .*callback present=1 type=1 size=2 desc=use-after-poison address=ok text=ok.ABORTING.death callback"
            -P ${LIBS_INCLUDE_DIR}/tests/AddressSanitizerDiagnostic.cmake)
        foreach(mode load4 load16 storeN unaligned partial report_error)
            set_tests_properties(guest_addresssanitizer_report_${mode} PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
        endforeach()
        set_tests_properties(guest_addresssanitizer_shadow PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAddressSanitizer>")
    endif()
endif()
