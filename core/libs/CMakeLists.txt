if(NOT WIN32)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(CheckPIESupported)
    check_pie_supported(LANGUAGES C CXX)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(APPLE)
    set(ANYPS5_PRX_RPATH "@loader_path")
else()
    set(ANYPS5_PRX_RPATH "$ORIGIN")
endif()

add_executable(nid_patcher
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ExportExclusions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ElfNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/PeNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/MachONidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/BinaryPatcherFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)

target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

if(MINGW)
    target_link_options(nid_patcher PRIVATE -municode -static -static-libgcc -static-libstdc++)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS -fno-asynchronous-unwind-tables)
    check_cxx_source_compiles("#ifdef __USING_SJLJ_EXCEPTIONS__\n#error\n#endif\nint main() { return 0; }" ANYPS5_MINGW_DWARF_EXCEPTIONS)
    unset(CMAKE_REQUIRED_FLAGS)
    if(NOT ANYPS5_MINGW_DWARF_EXCEPTIONS)
        message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} selects SjLj exceptions with -fno-asynchronous-unwind-tables; the libraries need DWARF unwinding. Use the MinGW-w64 toolchain listed in docs/dev/BUILD.md.")
    endif()
    add_compile_options(-fno-asynchronous-unwind-tables)
endif()

function(configure_windows_unwind target)
    if(MINGW)
        if(NOT EXISTS "${CMAKE_OBJCOPY}")
            message(FATAL_ERROR "Windows DWARF unwinding requires objcopy")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" --rename-section .eh_frame=.ehfram "$<TARGET_FILE:${target}>"
                VERBATIM
        )
    endif()
endfunction()

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()

    if(NOT EXISTS "${CMAKE_RANLIB}")
        message(FATAL_ERROR "Stable import libraries require ranlib")
    endif()
    list(LENGTH CMAKE_CXX_CREATE_SHARED_LIBRARY linkCommandCount)
    string(FIND "${CMAKE_CXX_CREATE_SHARED_LIBRARY}" "-Wl,--out-implib,<TARGET_IMPLIB> " implibOption)
    if(NOT linkCommandCount EQUAL 1 OR implibOption EQUAL -1)
        message(FATAL_ERROR "Expected one shared library link command that writes -Wl,--out-implib,<TARGET_IMPLIB>: ${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
    endif()
    string(REPLACE "-Wl,--out-implib,<TARGET_IMPLIB> " "-Wl,--out-implib,<TARGET_IMPLIB>.new " linkCommand "${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
    set(CMAKE_CXX_CREATE_SHARED_LIBRARY
            "\"${CMAKE_COMMAND}\" -E env SOURCE_DATE_EPOCH=0 ${linkCommand}"
            "\"${CMAKE_RANLIB}\" -D <TARGET_IMPLIB>.new"
            "\"${CMAKE_COMMAND}\" -E copy_if_different <TARGET_IMPLIB>.new <TARGET_IMPLIB>"
    )
endif()

set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc)
add_library(libc SHARED EXCLUDE_FROM_ALL)
set(LIBS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(${LIBC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/prx/libc)
target_include_directories(libc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
configure_windows_unwind(libc)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libc PRIVATE LINKER:-z,now)
    target_link_options(libc INTERFACE LINKER:--no-as-needed)
endif()

set(UNPATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs/unpatched)
set(PATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs)
set(IMPLIB_DIR ${PATCHED_DIR}/implib)

set_target_properties(libc PROPERTIES
        PREFIX ""
        SUFFIX .prx
        RUNTIME_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${IMPLIB_DIR}
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "${ANYPS5_PRX_RPATH}"
)

set(LIBC_UNPATCHED ${UNPATCHED_DIR}/libc.prx)
set(LIBC_PATCHED ${PATCHED_DIR}/libc.prx)
set(LIBC_EXPORT_LIST ${CMAKE_CURRENT_BINARY_DIR}/libc.exports)

add_custom_command(
        OUTPUT ${LIBC_EXPORT_LIST}
        COMMAND $<TARGET_FILE:nid_patcher> --write-export-list ${LIBC_UNPATCHED} ${LIBC_EXPORT_LIST}
        DEPENDS nid_patcher libc ${LIBC_UNPATCHED}
        COMMENT "Listing libc exports"
)

add_custom_command(
        OUTPUT ${LIBC_PATCHED}
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBC_UNPATCHED} ${LIBC_PATCHED}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${LIBC_PATCHED}
        DEPENDS nid_patcher libc ${LIBC_UNPATCHED}
        COMMENT "Patching NIDs for libc"
)

file(GLOB LIBS_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/prx/*)
list(REMOVE_ITEM LIBS_DIRS ${LIBC_DIR})

set(PATCHED_OUTPUTS ${LIBC_PATCHED})

foreach(LIB_DIR ${LIBS_DIRS})
    if(IS_DIRECTORY ${LIB_DIR})
        get_filename_component(LIB_NAME ${LIB_DIR} NAME)

        if(EXISTS ${LIB_DIR}/CMakeLists.txt)
            set(LIBS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
            add_subdirectory(${LIB_DIR} ${CMAKE_CURRENT_BINARY_DIR}/prx/${LIB_NAME})
        else()
            add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL ${LIB_DIR}/Export.cpp)
            target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            configure_windows_unwind(${LIB_NAME})
        endif()

        if(LIB_NAME STREQUAL "libkernel")
            target_link_libraries(${LIB_NAME} PUBLIC libc)
        else()
            target_link_libraries(${LIB_NAME} PRIVATE libkernel PUBLIC libc)
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${LIB_NAME} PRIVATE LINKER:-z,now)
        endif()
        if(WIN32 AND LIB_NAME STREQUAL "libSceNet")
            target_link_libraries(${LIB_NAME} PRIVATE ws2_32)
            target_compile_definitions(${LIB_NAME} PRIVATE _WIN32_WINNT=0x0602)
        elseif(WIN32 AND LIB_NAME STREQUAL "libSceNetCtl")
            target_link_libraries(${LIB_NAME} PRIVATE ws2_32 iphlpapi)
            target_compile_definitions(${LIB_NAME} PRIVATE _WIN32_WINNT=0x0602)
        endif()

        set_target_properties(${LIB_NAME} PROPERTIES
                PREFIX ""
                SUFFIX .prx
                RUNTIME_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
                LIBRARY_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
                ARCHIVE_OUTPUT_DIRECTORY ${IMPLIB_DIR}
                BUILD_WITH_INSTALL_RPATH ON
                INSTALL_RPATH "${ANYPS5_PRX_RPATH}"
        )

        set(UNPATCHED_FILE ${UNPATCHED_DIR}/${LIB_NAME}.prx)
        set(PATCHED_FILE ${PATCHED_DIR}/${LIB_NAME}.prx)

        add_custom_command(
                OUTPUT ${PATCHED_FILE}
                COMMAND ${CMAKE_COMMAND} -E copy ${UNPATCHED_FILE} ${PATCHED_FILE}
                COMMAND $<TARGET_FILE:nid_patcher> ${LIB_NAME} --preserve-export-list "${LIBC_EXPORT_LIST}" "${PATCHED_FILE}"
                DEPENDS nid_patcher ${LIB_NAME} ${UNPATCHED_FILE} ${LIBC_EXPORT_LIST}
                COMMENT "Patching NIDs for ${LIB_NAME}"
        )

        list(APPEND PATCHED_OUTPUTS ${PATCHED_FILE})
    endif()
endforeach()

if(APS5_ENABLE_TRACY)
    set_target_properties(TracyClient PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
            LIBRARY_OUTPUT_DIRECTORY ${UNPATCHED_DIR}
            ARCHIVE_OUTPUT_DIRECTORY ${IMPLIB_DIR}
    )
    set(TRACY_CLIENT_COPIED ${CMAKE_CURRENT_BINARY_DIR}/TracyClient.copied)
    add_custom_command(
            OUTPUT ${TRACY_CLIENT_COPIED}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PATCHED_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TracyClient> ${PATCHED_DIR}
            COMMAND ${CMAKE_COMMAND} -E touch ${TRACY_CLIENT_COPIED}
            DEPENDS TracyClient
            COMMENT "Copying the Tracy client next to the patched libraries"
    )
    list(APPEND PATCHED_OUTPUTS ${TRACY_CLIENT_COPIED})
endif()

add_custom_target(libs DEPENDS ${PATCHED_OUTPUTS})

if(BUILD_TESTING)
    add_test_executable(guest_posix_scalar_io_tests tests/GuestPosixScalarIo.cpp)
    target_include_directories(guest_posix_scalar_io_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_posix_scalar_io_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_posix_scalar_io_tests)
    add_test(NAME guest_posix_scalar_io COMMAND guest_posix_scalar_io_tests)
    if(WIN32)
        set_tests_properties(guest_posix_scalar_io PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
    add_test_executable(nid_buffer_validation_tests nid/tests/BufferValidation.cpp)
    target_include_directories(nid_buffer_validation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
    target_compile_features(nid_buffer_validation_tests PRIVATE cxx_std_20)
    if(MINGW)
        target_compile_options(nid_buffer_validation_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME nid_buffer_validation COMMAND nid_buffer_validation_tests)
    set_tests_properties(nid_buffer_validation PROPERTIES TIMEOUT 10)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_gnu_hash COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_gnu_hash.py $<TARGET_FILE:nid_patcher>)
        add_test(NAME nid_elf_duplicates COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_elf_duplicates.py $<TARGET_FILE:nid_patcher>)
        add_test(NAME nid_patcher_macho COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_macho_nids.py $<TARGET_FILE:nid_patcher>)
        add_test(NAME nid_patcher_native_paths COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_native_paths.py $<TARGET_FILE:nid_patcher>)
        set_tests_properties(nid_patcher_native_paths PROPERTIES TIMEOUT 20)
    endif()
    add_test_executable(patched_libraries_tests tests/PatchedLibraries.cpp)
    add_dependencies(patched_libraries_tests libs)
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_pe_references COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_pe_references.py $<TARGET_FILE:nid_patcher>)
    endif()
    if(MINGW)
        target_compile_options(patched_libraries_tests PRIVATE -fasynchronous-unwind-tables)
        target_link_options(patched_libraries_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME patched_libraries COMMAND patched_libraries_tests ${PATCHED_DIR})
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_elf_layout COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_elf_layout.py $<TARGET_FILE:nid_patcher>)
    endif()
    add_test_executable(guest_kernel_sys_tests tests/GuestKernelSys.cpp)
    target_include_directories(guest_kernel_sys_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_tests)
    add_test(NAME guest_kernel_sys COMMAND guest_kernel_sys_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_verneed COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_verneed.py $<TARGET_FILE:nid_patcher>)
    endif()
    if(WIN32)
        set_tests_properties(guest_kernel_sys PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()
    if(APPLE)
        add_subdirectory(tests/write_watch)
        add_subdirectory(tests/guest_arena)
    endif()
    if(WIN32)
        add_test_executable(windows_write_watch_tests tests/WindowsWriteWatch.cpp)
        target_include_directories(windows_write_watch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(windows_write_watch_tests PRIVATE _WIN32_WINNT=0x0A00)
        if(MINGW)
            target_compile_options(windows_write_watch_tests PRIVATE -fasynchronous-unwind-tables)
        endif()
        add_test(NAME windows_write_watch COMMAND windows_write_watch_tests)
        set_tests_properties(windows_write_watch PROPERTIES TIMEOUT 20)
    endif()

    if(WIN32)
        add_test_executable(undeliverable_host_exception_tests tests/UndeliverableHostException.cpp)
        add_dependencies(undeliverable_host_exception_tests libc)
        add_test(NAME undeliverable_host_exception COMMAND undeliverable_host_exception_tests $<TARGET_FILE:libc>)
        set_tests_properties(undeliverable_host_exception PROPERTIES
            PASS_REGULAR_EXPRESSION "C\\+\\+ exception St9bad_alloc raised on thread [0-9]+ cannot be delivered: 0x[0-9a-f]+ undeliverable_host_exception_tests\\.exe\\+0x[0-9a-f]+ has no unwind data")
    endif()

    add_test_executable(crash_text_tests tests/CrashText.cpp)
    target_include_directories(crash_text_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MINGW)
        target_compile_options(crash_text_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME crash_text COMMAND crash_text_tests)

    if(WIN32)
        add_test_executable(windows_crash_text_tests tests/WindowsCrashText.cpp)
        add_dependencies(windows_crash_text_tests libc)
        if(MINGW)
            target_compile_options(windows_crash_text_tests PRIVATE -fasynchronous-unwind-tables)
        endif()
        add_test(NAME windows_crash_text COMMAND windows_crash_text_tests $<TARGET_FILE:libc>)
        set_tests_properties(windows_crash_text PROPERTIES TIMEOUT 20
            PASS_REGULAR_EXPRESSION "faulting[^~]*FATAL: unhandled exception 0xc0000005[^~]*  \\[r12\\] \"allocation failed in the heap\"[\r\n]+  \\[r13\\] u\"LowLevelFatalError \\[File:Unknown\\] \\[Line: 42\\] Out of memory\"[\r\n]+  \\[r14\\] \"PAGE END\"[.][.][.]"
            FAIL_REGULAR_EXPRESSION "\\[rbx\\] u?\"|did not fault")
    endif()

    add_test_executable(guest_voice_port_attr_tests tests/GuestVoicePortAttr.cpp)
    target_include_directories(guest_voice_port_attr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_voice_port_attr_tests PRIVATE libSceVoice libc)
    configure_windows_unwind(guest_voice_port_attr_tests)
    add_test(NAME guest_voice_port_attr COMMAND guest_voice_port_attr_tests)
    if(WIN32)
        set_tests_properties(guest_voice_port_attr PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVoice>")
    endif()

    add_test_executable(guest_offline_imports_tests tests/GuestOfflineImports.cpp)
    target_include_directories(guest_offline_imports_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_offline_imports_tests PRIVATE libSceHttp libSceNpEntitlementAccess libSceRudp libc)
    configure_windows_unwind(guest_offline_imports_tests)
    add_test(NAME guest_offline_imports COMMAND guest_offline_imports_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_shared_strings COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_shared_strings.py $<TARGET_FILE:nid_patcher>)
    endif()
    if(WIN32)
        set_tests_properties(guest_offline_imports PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceHttp>")
    endif()

    add_test_executable(guest_signin_dialog_tests tests/GuestSigninDialog.cpp)
    target_include_directories(guest_signin_dialog_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_signin_dialog_tests PRIVATE libSceSigninDialog libc)
    configure_windows_unwind(guest_signin_dialog_tests)
    add_test(NAME guest_signin_dialog COMMAND guest_signin_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_signin_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSigninDialog>")
    endif()

    add_test_executable(guest_netctl_ap_dialog_tests tests/GuestNetCtlApDialog.cpp)
    target_include_directories(guest_netctl_ap_dialog_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_netctl_ap_dialog_tests PRIVATE libSceNetCtlApDialog libc)
    configure_windows_unwind(guest_netctl_ap_dialog_tests)
    add_test(NAME guest_netctl_ap_dialog COMMAND guest_netctl_ap_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_netctl_ap_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNetCtlApDialog>")
    endif()

    add_test_executable(guest_np_auth_tests tests/GuestNpAuth.cpp)
    target_include_directories(guest_np_auth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_auth_tests PRIVATE libSceNpAuth libc)
    configure_windows_unwind(guest_np_auth_tests)
    add_test(NAME guest_np_auth COMMAND guest_np_auth_tests)
    if(WIN32)
        set_tests_properties(guest_np_auth PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpAuth>")
    endif()

    add_test_executable(guest_rudp_status_tests tests/GuestRudpStatus.cpp)
    target_include_directories(guest_rudp_status_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rudp_status_tests PRIVATE libSceRudp libc)
    configure_windows_unwind(guest_rudp_status_tests)
    add_test(NAME guest_rudp_status COMMAND guest_rudp_status_tests)
    if(WIN32)
        set_tests_properties(guest_rudp_status PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRudp>")
    endif()

    add_test_executable(guest_rudp_context_queries_tests tests/GuestRudpContextQueries.cpp)
    target_include_directories(guest_rudp_context_queries_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rudp_context_queries_tests PRIVATE libSceRudp libc)
    configure_windows_unwind(guest_rudp_context_queries_tests)
    add_test(NAME guest_rudp_context_queries COMMAND guest_rudp_context_queries_tests)
    if(WIN32)
        set_tests_properties(guest_rudp_context_queries PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRudp>")
    endif()

    add_test_executable(guest_rudp_pump_tests tests/GuestRudpPump.cpp)
    target_include_directories(guest_rudp_pump_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rudp_pump_tests PRIVATE libSceRudp libc)
    configure_windows_unwind(guest_rudp_pump_tests)
    add_test(NAME guest_rudp_pump COMMAND guest_rudp_pump_tests)
    if(WIN32)
        set_tests_properties(guest_rudp_pump PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRudp>")
    endif()

    add_test_executable(guest_memory_pool_stats_tests tests/GuestMemoryPoolStats.cpp)
    target_include_directories(guest_memory_pool_stats_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_memory_pool_stats_tests PRIVATE libSceSsl libSceHttp2 libSceNet libkernel libc)
    configure_windows_unwind(guest_memory_pool_stats_tests)
    add_test(NAME guest_memory_pool_stats COMMAND guest_memory_pool_stats_tests)
    if(WIN32)
        set_tests_properties(guest_memory_pool_stats PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSsl>")
    endif()

    add_test_executable(guest_system_gesture_tests tests/GuestSystemGesture.cpp)
    target_include_directories(guest_system_gesture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_system_gesture_tests PRIVATE libSceSystemGesture libc)
    configure_windows_unwind(guest_system_gesture_tests)
    add_test(NAME guest_system_gesture COMMAND guest_system_gesture_tests)
    if(WIN32)
        set_tests_properties(guest_system_gesture PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSystemGesture>")
    endif()

    if(UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64"))
        add_subdirectory(tests/register_context)
    endif()

    add_test_executable(savedata_memory_metadata_tests tests/SaveDataMemoryMetadata.cpp)
    target_include_directories(savedata_memory_metadata_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(savedata_memory_metadata_tests PRIVATE libSceSaveData.native)
    configure_windows_unwind(savedata_memory_metadata_tests)
    add_test(NAME savedata_memory_metadata COMMAND savedata_memory_metadata_tests)
    if(MINGW)
        target_compile_options(savedata_memory_metadata_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    if(WIN32)
        set_tests_properties(savedata_memory_metadata PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()
    add_test_executable(savedata_memory_growth_tests tests/SaveDataMemoryGrowth.cpp)
    target_include_directories(savedata_memory_growth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(savedata_memory_growth_tests PRIVATE libSceSaveData.native)
    configure_windows_unwind(savedata_memory_growth_tests)
    if(MINGW)
        target_compile_options(savedata_memory_growth_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME savedata_memory_growth COMMAND savedata_memory_growth_tests)
    add_test(NAME savedata_memory_growth_read_failure COMMAND savedata_memory_growth_tests read-failure)
    set_tests_properties(savedata_memory_growth_read_failure PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(savedata_memory_growth savedata_memory_growth_read_failure PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()
    add_library(host_thread_local_test_library SHARED tests/HostThreadLocalLibrary.cpp)
    set_target_properties(host_thread_local_test_library PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")
    target_include_directories(host_thread_local_test_library PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test_executable(host_thread_local_tests tests/HostThreadLocal.cpp)
    target_link_libraries(host_thread_local_tests PRIVATE host_thread_local_test_library)
    if(MINGW)
        target_compile_options(host_thread_local_test_library PRIVATE -fasynchronous-unwind-tables)
        target_compile_options(host_thread_local_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME host_thread_local COMMAND host_thread_local_tests)
    set_tests_properties(host_thread_local PROPERTIES TIMEOUT 15)
    if(MINGW)
        add_test(NAME prx_thread_local_destructors COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DOBJDUMP=${CMAKE_OBJDUMP}
            -DMODULES=$<TARGET_FILE_DIR:libc> -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ThreadLocalDestructors.cmake)
        set_tests_properties(prx_thread_local_destructors PROPERTIES TIMEOUT 300)
    endif()

    add_test_executable(savedata_core_write_replacement_tests tests/SaveDataWriteReplacement.cpp)
    target_include_directories(savedata_core_write_replacement_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(savedata_core_write_replacement_tests PRIVATE libSceSaveData)
    configure_windows_unwind(savedata_core_write_replacement_tests)
    add_test(NAME savedata_core_write_replacement COMMAND savedata_core_write_replacement_tests)

    add_test_executable(savedata_native_write_replacement_tests tests/SaveDataWriteReplacement.cpp)
    target_include_directories(savedata_native_write_replacement_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(savedata_native_write_replacement_tests PRIVATE SAVEDATA_NATIVE_BACKEND)
    target_link_libraries(savedata_native_write_replacement_tests PRIVATE libSceSaveData.native)
    configure_windows_unwind(savedata_native_write_replacement_tests)
    add_test(NAME savedata_native_write_replacement COMMAND savedata_native_write_replacement_tests)

    add_test_executable(savedata_replace_file_failure_tests tests/SaveDataFileWriteFailure.cpp)
    target_include_directories(savedata_replace_file_failure_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    configure_windows_unwind(savedata_replace_file_failure_tests)
    add_test(NAME savedata_replace_file_failure COMMAND savedata_replace_file_failure_tests)

    add_test_executable(guest_crt_tests tests/GuestCrt.cpp)
    target_include_directories(guest_crt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_crt_tests PRIVATE libSceAmpr libc)
    configure_windows_unwind(guest_crt_tests)
    add_test(NAME guest_crt COMMAND guest_crt_tests)
    if(WIN32)
        set_tests_properties(guest_crt PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAmpr>")
    endif()

    if(MINGW)
        target_compile_options(savedata_core_write_replacement_tests PRIVATE -fasynchronous-unwind-tables)
        target_compile_options(savedata_native_write_replacement_tests PRIVATE -fasynchronous-unwind-tables)
        target_compile_options(savedata_replace_file_failure_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    if(WIN32)
        set_tests_properties(savedata_core_write_replacement PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData>")
        set_tests_properties(savedata_native_write_replacement PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()

    add_test_executable(guest_posix_random_tests tests/GuestPosixRandom.cpp)
    target_include_directories(guest_posix_random_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_posix_random_tests PRIVATE libScePosixForWebKit)
    configure_windows_unwind(guest_posix_random_tests)
    add_test(NAME guest_posix_random COMMAND guest_posix_random_tests)
    if(WIN32)
        set_tests_properties(guest_posix_random PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePosixForWebKit>")
    endif()

    add_test_executable(MemoryPoolTest
        tests/MemoryPoolTest.cpp
        prx/libkernel/DirectMemory/MemoryPool.cpp
    )
    target_include_directories(MemoryPoolTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MINGW)
        target_compile_options(MemoryPoolTest PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME MemoryPoolTest COMMAND MemoryPoolTest)

    add_test_executable(guest_acm_tests tests/GuestAcm.cpp)
    target_include_directories(guest_acm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_acm_tests PRIVATE libSceAcm libc)
    configure_windows_unwind(guest_acm_tests)
    add_test(NAME guest_acm COMMAND guest_acm_tests)
    if(WIN32)
        set_tests_properties(guest_acm PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAcm>")
    endif()
endif()
