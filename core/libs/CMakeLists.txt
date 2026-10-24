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
        check_cxx_source_compiles("#if !defined(__SEH__) || defined(__USING_SJLJ_EXCEPTIONS__)\n#error\n#endif\n[[gnu::sysv_abi, gnu::noinline]] void Throw() { throw 1; }\n[[gnu::sysv_abi, gnu::noinline]] int Catch() { try { Throw(); } catch (int value) { return value; } return 0; }\nint main() { return Catch() != 1; }" ANYPS5_MINGW_SYSV_SEH_EXCEPTIONS)
        if(NOT ANYPS5_MINGW_SYSV_SEH_EXCEPTIONS)
            message(FATAL_ERROR "The Windows libraries require DWARF exceptions or GCC with System V ABI SEH support. Use the toolchains listed in docs/dev/BUILD.md.")
        endif()
        add_library(windows_seh_personalities OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc/src/specifics/windows/SehPersonalities.cpp)
        target_link_options(windows_seh_personalities INTERFACE LINKER:--undefined=LibcSehPersonalities_nid_no_patch)
        link_libraries(windows_seh_personalities)
    else()
        add_compile_options(-fno-asynchronous-unwind-tables)
    endif()
endif()

function(configure_windows_unwind target)
    if(MINGW)
        if(NOT EXISTS "${CMAKE_OBJCOPY}")
            message(FATAL_ERROR "Windows DWARF unwinding requires objcopy")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" --rename-section .eh_frame=.ehfram --rename-section .eh_fram=.ehfram "$<TARGET_FILE:${target}>"
                VERBATIM
        )
    endif()
endfunction()

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()

    if(NOT EXISTS "${CMAKE_RANLIB}")
        if(EXISTS "${CMAKE_CXX_COMPILER_RANLIB}")
            set(CMAKE_RANLIB "${CMAKE_CXX_COMPILER_RANLIB}")
        else()
            get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            if(EXISTS "${_compiler_dir}/gcc-ranlib${CMAKE_EXECUTABLE_SUFFIX}")
                set(CMAKE_RANLIB "${_compiler_dir}/gcc-ranlib${CMAKE_EXECUTABLE_SUFFIX}")
            elseif(EXISTS "${_compiler_dir}/ranlib${CMAKE_EXECUTABLE_SUFFIX}")
                set(CMAKE_RANLIB "${_compiler_dir}/ranlib${CMAKE_EXECUTABLE_SUFFIX}")
            endif()
        endif()
    endif()
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
if(WIN32)
    target_compile_options(libc PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc/include/HostStandardStreams.hpp")
endif()
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

        if(WIN32)
            target_compile_options(${LIB_NAME} PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc/include/HostStandardStreams.hpp")
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

target_link_libraries(libSceUsbStorageDialog PRIVATE libSceUserService)

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

if(TARGET mimalloc)
    add_executable(pe_import_first ${CMAKE_CURRENT_SOURCE_DIR}/allocator/PeImportFirstMain.cpp ${CMAKE_CURRENT_SOURCE_DIR}/allocator/PeImportFirst.cpp)
    target_compile_features(pe_import_first PRIVATE cxx_std_20)
    if(MINGW)
        target_compile_options(pe_import_first PRIVATE -fasynchronous-unwind-tables)
        target_link_options(pe_import_first PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    add_custom_command(
            OUTPUT ${PATCHED_DIR}/mimalloc.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mimalloc> ${PATCHED_DIR}/mimalloc.dll
            COMMAND $<TARGET_FILE:pe_import_first> ${PATCHED_DIR}/mimalloc.dll mimalloc-redirect.dll
            DEPENDS mimalloc pe_import_first
            COMMENT "Copying mimalloc.dll beside the libraries, its redirection module imported first"
    )
    add_custom_command(
            OUTPUT ${PATCHED_DIR}/mimalloc-redirect.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:mimalloc>/mimalloc-redirect.dll ${PATCHED_DIR}/mimalloc-redirect.dll
            DEPENDS mimalloc
            COMMENT "Copying mimalloc-redirect.dll beside the libraries"
    )
    list(APPEND PATCHED_OUTPUTS ${PATCHED_DIR}/mimalloc.dll ${PATCHED_DIR}/mimalloc-redirect.dll)
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
    add_test_executable(guest_kernel_sys_sdk_version_tests tests/GuestKernelSysSdkVersion.cpp)
    target_include_directories(guest_kernel_sys_sdk_version_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_sdk_version_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_sdk_version_tests)
    add_test(NAME guest_kernel_sys_sdk_version COMMAND guest_kernel_sys_sdk_version_tests)
    if(WIN32)
        set_tests_properties(guest_kernel_sys_sdk_version PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()
    add_test_executable(nid_buffer_validation_tests nid/tests/BufferValidation.cpp)
    target_include_directories(nid_buffer_validation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
    target_compile_features(nid_buffer_validation_tests PRIVATE cxx_std_20)
    if(MINGW)
        target_compile_options(nid_buffer_validation_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME nid_buffer_validation COMMAND nid_buffer_validation_tests)
    set_tests_properties(nid_buffer_validation PROPERTIES TIMEOUT 10)

    add_test_executable(pe_import_first_tests allocator/tests/PeImportFirstTests.cpp allocator/PeImportFirst.cpp)
    target_include_directories(pe_import_first_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(pe_import_first_tests PRIVATE cxx_std_20)
    if(MINGW)
        target_compile_options(pe_import_first_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME pe_import_first COMMAND pe_import_first_tests)
    set_tests_properties(pe_import_first PROPERTIES TIMEOUT 10)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        set(public_header_compiler_args)
        if(APPLE)
            foreach(architecture IN LISTS CMAKE_OSX_ARCHITECTURES)
                list(APPEND public_header_compiler_args --compiler-arg=-arch "--compiler-arg=${architecture}")
            endforeach()
            if(CMAKE_OSX_SYSROOT)
                list(APPEND public_header_compiler_args --compiler-arg=-isysroot "--compiler-arg=${CMAKE_OSX_SYSROOT}")
            endif()
        endif()
        set(public_header_manifest "${CMAKE_CURRENT_BINARY_DIR}/public_headers.json")
        add_test(NAME public_headers COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_public_headers.py --audit-only --manifest "${public_header_manifest}")
        set_tests_properties(public_headers PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE FIXTURES_SETUP public_headers_manifest)
        foreach(batch RANGE 0 63)
            add_test(NAME public_headers_compile_${batch} COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_public_headers.py --compiler ${CMAKE_CXX_COMPILER} ${public_header_compiler_args}
                --manifest "${public_header_manifest}" --batch-index ${batch} --batch-count 64)
            set_tests_properties(public_headers_compile_${batch} PROPERTIES TIMEOUT 30 FIXTURES_REQUIRED public_headers_manifest)
        endforeach()
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
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_library(host_libstdcxx_exception_module SHARED tests/HostLibstdcxxExceptionModule.cpp)
        target_link_libraries(host_libstdcxx_exception_module PRIVATE ${PATCHED_DIR}/libc.prx)
        add_dependencies(host_libstdcxx_exception_module libs)
        add_test_executable(host_libstdcxx_exception_tests tests/HostLibstdcxxException.cpp)
        target_link_libraries(host_libstdcxx_exception_tests PRIVATE host_libstdcxx_exception_module ${PATCHED_DIR}/libc.prx)
        add_dependencies(host_libstdcxx_exception_tests libs)
        set_target_properties(host_libstdcxx_exception_module host_libstdcxx_exception_tests PROPERTIES BUILD_RPATH ${PATCHED_DIR})
        add_test(NAME host_libstdcxx_exception COMMAND host_libstdcxx_exception_tests)
        set_tests_properties(host_libstdcxx_exception PROPERTIES TIMEOUT 20)
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_elf_layout COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_elf_layout.py $<TARGET_FILE:nid_patcher>)
    endif()
    add_test_executable(guest_kernel_sys_tests tests/GuestKernelSys.cpp)
    target_include_directories(guest_kernel_sys_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_tests)
    add_test(NAME guest_kernel_sys COMMAND guest_kernel_sys_tests)
    add_test_executable(guest_kernel_sys_thread_defaults_tests tests/GuestKernelSysThreadDefaults.cpp)
    target_include_directories(guest_kernel_sys_thread_defaults_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_thread_defaults_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_thread_defaults_tests)
    add_test(NAME guest_kernel_sys_thread_defaults COMMAND guest_kernel_sys_thread_defaults_tests)
    set_tests_properties(guest_kernel_sys_thread_defaults PROPERTIES TIMEOUT 20)
    add_test_executable(guest_kernel_sys_barrier_tests tests/GuestKernelSysBarrier.cpp)
    target_include_directories(guest_kernel_sys_barrier_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_barrier_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_barrier_tests)
    add_test(NAME guest_kernel_sys_barrier COMMAND guest_kernel_sys_barrier_tests)
    set_tests_properties(guest_kernel_sys_barrier PROPERTIES TIMEOUT 20)
    add_test_executable(guest_kernel_sys_pthread_tests tests/GuestKernelSysPthread.cpp)
    target_include_directories(guest_kernel_sys_pthread_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_pthread_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_pthread_tests)
    add_test(NAME guest_kernel_sys_pthread COMMAND guest_kernel_sys_pthread_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME nid_verneed COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/test_verneed.py $<TARGET_FILE:nid_patcher>)
    endif()
    if(WIN32)
        set_tests_properties(guest_kernel_sys guest_kernel_sys_thread_defaults guest_kernel_sys_barrier guest_kernel_sys_pthread PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()

    add_test_executable(guest_gnm_neo_packets_tests tests/GuestGnmNeoPackets.cpp)
    target_include_directories(guest_gnm_neo_packets_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_gnm_neo_packets_tests PRIVATE libSceGnmDriverForNeoMode libc)
    configure_windows_unwind(guest_gnm_neo_packets_tests)
    add_test(NAME guest_gnm_neo_packets COMMAND guest_gnm_neo_packets_tests)
    if(WIN32)
        set_tests_properties(guest_gnm_neo_packets PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriverForNeoMode>")
    endif()
    if(APPLE)
        add_subdirectory(tests/write_watch)
        add_subdirectory(tests/guest_arena)
    endif()
    add_test_executable(guest_kernel_sys_module_internal_tests tests/GuestKernelSysModuleInternal.cpp)
    target_include_directories(guest_kernel_sys_module_internal_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_module_internal_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_module_internal_tests)
    add_test(NAME guest_kernel_sys_module_internal COMMAND guest_kernel_sys_module_internal_tests)
    set_tests_properties(guest_kernel_sys_module_internal PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_sys_module_internal PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()
    if(WIN32)
        add_test_executable(windows_write_watch_tests tests/WindowsWriteWatch.cpp)
        target_include_directories(windows_write_watch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(windows_write_watch_tests PRIVATE _WIN32_WINNT=0x0A00 APS5_COUNT_MAPPING_QUERIES=1)
        if(MINGW)
            target_compile_options(windows_write_watch_tests PRIVATE -fasynchronous-unwind-tables)
        endif()
        add_test(NAME windows_write_watch COMMAND windows_write_watch_tests)
        set_tests_properties(windows_write_watch PROPERTIES TIMEOUT 20)
    endif()

    if(WIN32)
        add_test_executable(undeliverable_host_exception_tests tests/UndeliverableHostException.cpp)
        if(MINGW)
            target_compile_options(undeliverable_host_exception_tests PRIVATE -fno-asynchronous-unwind-tables -fno-exceptions)
        endif()
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
    add_test_executable(guest_kernel_sys_pselect_tests tests/GuestKernelSysPselect.cpp)
    target_include_directories(guest_kernel_sys_pselect_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_pselect_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_pselect_tests)
    add_test(NAME guest_kernel_sys_pselect COMMAND guest_kernel_sys_pselect_tests)
    set_tests_properties(guest_kernel_sys_pselect PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_sys_pselect PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()

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

    add_test_executable(guest_gnm_neo_retail_tests tests/GuestGnmNeoRetail.cpp)
    target_include_directories(guest_gnm_neo_retail_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_gnm_neo_retail_tests PRIVATE libSceGnmDriverForNeoMode libc)
    configure_windows_unwind(guest_gnm_neo_retail_tests)
    add_test(NAME guest_gnm_neo_retail COMMAND guest_gnm_neo_retail_tests)
    if(WIN32)
        set_tests_properties(guest_gnm_neo_retail PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriverForNeoMode>")
    endif()

    add_test_executable(guest_vr_tracker_devices_tests tests/GuestVrTrackerDevices.cpp)
    target_include_directories(guest_vr_tracker_devices_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_vr_tracker_devices_tests PRIVATE libSceVrTracker libc)
    configure_windows_unwind(guest_vr_tracker_devices_tests)
    add_test(NAME guest_vr_tracker_devices COMMAND guest_vr_tracker_devices_tests)
    if(WIN32)
        set_tests_properties(guest_vr_tracker_devices PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVrTracker>")
    endif()

    add_test_executable(guest_move_tests tests/GuestMove.cpp)
    target_include_directories(guest_move_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_move_tests PRIVATE libSceMove libc)
    configure_windows_unwind(guest_move_tests)
    add_test(NAME guest_move COMMAND guest_move_tests)
    if(WIN32)
        set_tests_properties(guest_move PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceMove>")
    endif()

    add_test_executable(guest_np_party_tests tests/GuestNpParty.cpp)
    target_include_directories(guest_np_party_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_party_tests PRIVATE libSceNpParty libc)
    configure_windows_unwind(guest_np_party_tests)
    add_test(NAME guest_np_party COMMAND guest_np_party_tests)
    if(WIN32)
        set_tests_properties(guest_np_party PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpParty>")
    endif()

    add_test_executable(guest_np_ps4_imports_tests tests/GuestNpPs4Imports.cpp)
    target_include_directories(guest_np_ps4_imports_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_ps4_imports_tests PRIVATE libSceNpMatching2 libSceNpSignaling libSceNpTrophy libc)
    configure_windows_unwind(guest_np_ps4_imports_tests)
    add_test(NAME guest_np_ps4_imports COMMAND guest_np_ps4_imports_tests)
    if(WIN32)
        set_tests_properties(guest_np_ps4_imports PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpMatching2>")
    endif()

    add_test_executable(guest_np_common_tests tests/GuestNpCommon.cpp)
    target_include_directories(guest_np_common_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_common_tests PRIVATE libSceNpCommon libkernel libc)
    configure_windows_unwind(guest_np_common_tests)
    foreach(testCase IN ITEMS ids sync clock)
        add_test(NAME guest_np_common_${testCase} COMMAND guest_np_common_tests ${testCase})
        set_tests_properties(guest_np_common_${testCase} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(guest_np_common_${testCase} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpCommon>")
        endif()
    endforeach()

    add_test_executable(guest_np_common_runtime_tests tests/GuestNpCommonRuntime.cpp)
    target_include_directories(guest_np_common_runtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_common_runtime_tests PRIVATE libSceNpCommon libkernel libc)
    configure_windows_unwind(guest_np_common_runtime_tests)
    add_test(NAME guest_np_common_runtime COMMAND guest_np_common_runtime_tests)
    set_tests_properties(guest_np_common_runtime PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_np_common_runtime PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpCommon>")
    endif()

    add_test_executable(guest_offline_imports_tests tests/GuestOfflineImports.cpp)
    target_include_directories(guest_offline_imports_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_offline_imports_tests PRIVATE libSceHttp libSceNpEntitlementAccess libSceRudp libSceVideoRecordingP libc)
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

    add_test_executable(guest_entitlements_tests tests/GuestEntitlements.cpp)
    target_include_directories(guest_entitlements_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_entitlements_tests PRIVATE libSceNpEntitlementAccess libc)
    configure_windows_unwind(guest_entitlements_tests)
    add_test(NAME guest_entitlements COMMAND guest_entitlements_tests)
    add_test(NAME guest_entitlements_missing COMMAND guest_entitlements_tests missing)
    if(WIN32)
        set_tests_properties(guest_entitlements guest_entitlements_missing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpEntitlementAccess>")
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

    add_test_executable(guest_usb_storage_dialog_tests tests/GuestUsbStorageDialog.cpp)
    target_include_directories(guest_usb_storage_dialog_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_usb_storage_dialog_tests PRIVATE libSceUsbStorageDialog libSceUserService libc)
    configure_windows_unwind(guest_usb_storage_dialog_tests)
    add_test(NAME guest_usb_storage_dialog COMMAND guest_usb_storage_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_usb_storage_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceUsbStorageDialog>")
    endif()

    add_test_executable(guest_game_live_streaming_tests tests/GuestGameLiveStreaming.cpp)
    target_include_directories(guest_game_live_streaming_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_game_live_streaming_tests PRIVATE libSceGameLiveStreaming libc)
    configure_windows_unwind(guest_game_live_streaming_tests)
    add_test(NAME guest_game_live_streaming COMMAND guest_game_live_streaming_tests)
    if(WIN32)
        set_tests_properties(guest_game_live_streaming PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGameLiveStreaming>")
    endif()

    add_test_executable(guest_netctl_ap_tests tests/GuestNetCtlAp.cpp)
    target_include_directories(guest_netctl_ap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_netctl_ap_tests PRIVATE libSceNetCtlAp libc)
    configure_windows_unwind(guest_netctl_ap_tests)
    add_test(NAME guest_netctl_ap COMMAND guest_netctl_ap_tests)
    if(WIN32)
        set_tests_properties(guest_netctl_ap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNetCtlAp>")
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

    add_test_executable(guest_rudp_context_lifecycle_tests tests/GuestRudpContextLifecycle.cpp)
    target_include_directories(guest_rudp_context_lifecycle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rudp_context_lifecycle_tests PRIVATE libSceRudp libc)
    configure_windows_unwind(guest_rudp_context_lifecycle_tests)
    add_test(NAME guest_rudp_context_lifecycle COMMAND guest_rudp_context_lifecycle_tests)
    if(WIN32)
        set_tests_properties(guest_rudp_context_lifecycle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRudp>")
    endif()

    add_test_executable(guest_rudp_poll_tests tests/GuestRudpPoll.cpp)
    target_include_directories(guest_rudp_poll_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_rudp_poll_tests PRIVATE libSceRudp libc)
    configure_windows_unwind(guest_rudp_poll_tests)
    add_test(NAME guest_rudp_poll COMMAND guest_rudp_poll_tests)
    if(WIN32)
        set_tests_properties(guest_rudp_poll PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(savedata_memory_journal_tests tests/SaveDataMemoryJournal.cpp)
    target_include_directories(savedata_memory_journal_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(savedata_memory_journal_tests PRIVATE libSceSaveData.native)
    configure_windows_unwind(savedata_memory_journal_tests)
    if(MINGW)
        target_compile_options(savedata_memory_journal_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME savedata_memory_journal COMMAND savedata_memory_journal_tests)
    if(WIN32)
        set_tests_properties(savedata_memory_journal PROPERTIES
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

    add_test_executable(guest_np_cpp_web_api_intrusive_ptr_tests tests/GuestNpCppWebApiIntrusivePtr.cpp)
    target_include_directories(guest_np_cpp_web_api_intrusive_ptr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_cpp_web_api_intrusive_ptr_tests PRIVATE libSceNpCppWebApi libc)
    configure_windows_unwind(guest_np_cpp_web_api_intrusive_ptr_tests)
    add_test(NAME guest_np_cpp_web_api_intrusive_ptr COMMAND guest_np_cpp_web_api_intrusive_ptr_tests)
    if(WIN32)
        set_tests_properties(guest_np_cpp_web_api_intrusive_ptr PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpCppWebApi>")
    endif()

    add_test_executable(guest_np_cpp_web_api_signed_out_tests tests/GuestNpCppWebApiSignedOut.cpp)
    target_include_directories(guest_np_cpp_web_api_signed_out_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_cpp_web_api_signed_out_tests PRIVATE libSceNpCppWebApi libc)
    configure_windows_unwind(guest_np_cpp_web_api_signed_out_tests)
    add_test(NAME guest_np_cpp_web_api_signed_out COMMAND guest_np_cpp_web_api_signed_out_tests)
    if(WIN32)
        set_tests_properties(guest_np_cpp_web_api_signed_out PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpCppWebApi>")
    endif()

    add_test_executable(guest_np_cpp_web_api_offline_tests tests/GuestNpCppWebApiOffline.cpp)
    target_include_directories(guest_np_cpp_web_api_offline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_cpp_web_api_offline_tests PRIVATE libSceNpCppWebApi libc)
    configure_windows_unwind(guest_np_cpp_web_api_offline_tests)
    add_test(NAME guest_np_cpp_web_api_offline COMMAND guest_np_cpp_web_api_offline_tests)
    if(WIN32)
        set_tests_properties(guest_np_cpp_web_api_offline PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpCppWebApi>")
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

    add_test_executable(guest_kernel_sys_ktimer_tests tests/GuestKernelSysKtimer.cpp)
    target_include_directories(guest_kernel_sys_ktimer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_ktimer_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_ktimer_tests)
    add_test(NAME guest_kernel_sys_ktimer COMMAND guest_kernel_sys_ktimer_tests)
    set_tests_properties(guest_kernel_sys_ktimer PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_sys_ktimer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()

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

    add_test_executable(guest_kernel_sys_aio_tests tests/GuestKernelSysAio.cpp)
    target_include_directories(guest_kernel_sys_aio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_kernel_sys_aio_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_kernel_sys_aio_tests)
    add_test(NAME guest_kernel_sys_aio COMMAND guest_kernel_sys_aio_tests)
    set_tests_properties(guest_kernel_sys_aio PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_kernel_sys_aio PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
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

    add_test_executable(guest_voice_chat_tests tests/GuestVoiceChat.cpp)
    target_include_directories(guest_voice_chat_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_voice_chat_tests PRIVATE libSceVoiceChat libc)
    configure_windows_unwind(guest_voice_chat_tests)
    add_test(NAME guest_voice_chat COMMAND guest_voice_chat_tests)
    if(WIN32)
        set_tests_properties(guest_voice_chat PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVoiceChat>")
    endif()

    add_test_executable(guest_gnm_neo_embedded_tests tests/GuestGnmNeoEmbedded.cpp)
    target_include_directories(guest_gnm_neo_embedded_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_gnm_neo_embedded_tests PRIVATE libSceGnmDriverForNeoMode libc)
    configure_windows_unwind(guest_gnm_neo_embedded_tests)
    add_test(NAME guest_gnm_neo_embedded COMMAND guest_gnm_neo_embedded_tests)
    if(WIN32)
        set_tests_properties(guest_gnm_neo_embedded PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriverForNeoMode>")
    endif()

    add_test_executable(guest_np_game_intent_tests tests/GuestNpGameIntent.cpp)
    target_include_directories(guest_np_game_intent_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_np_game_intent_tests PRIVATE libSceNpGameIntent libc)
    configure_windows_unwind(guest_np_game_intent_tests)
    add_test(NAME guest_np_game_intent COMMAND guest_np_game_intent_tests)
    if(WIN32)
        set_tests_properties(guest_np_game_intent PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpGameIntent>")
    endif()

    add_test_executable(guest_gnm_neo_shaders_tests tests/GuestGnmNeoShaders.cpp)
    target_include_directories(guest_gnm_neo_shaders_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_gnm_neo_shaders_tests PRIVATE libSceGnmDriverForNeoMode libc)
    configure_windows_unwind(guest_gnm_neo_shaders_tests)
    add_test(NAME guest_gnm_neo_shaders COMMAND guest_gnm_neo_shaders_tests)
    if(WIN32)
        set_tests_properties(guest_gnm_neo_shaders PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriverForNeoMode>")
    endif()

    add_test_executable(guest_vorbis_dec_tests tests/GuestVorbisDec.cpp)
    target_include_directories(guest_vorbis_dec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(guest_vorbis_dec_tests PRIVATE OV_EXCLUDE_STATIC_CALLBACKS)
    target_link_libraries(guest_vorbis_dec_tests PRIVATE libSceVorbisDec vorbisenc vorbisfile vorbis ogg libc)
    configure_windows_unwind(guest_vorbis_dec_tests)
    add_test(NAME guest_vorbis_dec COMMAND guest_vorbis_dec_tests)
    set_tests_properties(guest_vorbis_dec PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_vorbis_dec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVorbisDec>")
    endif()

    add_test_executable(guest_gnm_neo_default_state_tests tests/GuestGnmNeoDefaultState.cpp)
    target_include_directories(guest_gnm_neo_default_state_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(guest_gnm_neo_default_state_tests PRIVATE libSceGnmDriverForNeoMode libc)
    configure_windows_unwind(guest_gnm_neo_default_state_tests)
    add_test(NAME guest_gnm_neo_default_state COMMAND guest_gnm_neo_default_state_tests)
    if(WIN32)
        set_tests_properties(guest_gnm_neo_default_state PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriverForNeoMode>")
    endif()
endif()
