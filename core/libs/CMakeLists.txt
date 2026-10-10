if(NOT WIN32)
    string(REPLACE "-static " "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ")
    string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
endif()

add_executable(nid_patcher
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ExportExclusions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ElfNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/PeNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/BinaryPatcherFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)

target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

if(BUILD_TESTING)
    anyps5_add_test(nid_unit_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/BinaryPatcherFactoryTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/ExportExclusionsTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/NidComputeTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/NidPatcherUtilsTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/NidResolverTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/PeNidPatcherTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/tests/unit/Sha1Tests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidResolver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ExportExclusions.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ElfNidPatcher.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/PeNidPatcher.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/BinaryPatcherFactory.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/nid/include
    )
    if(MINGW)
        target_link_options(nid_unit_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

if(MINGW)
    target_link_options(nid_patcher PRIVATE -static -static-libgcc -static-libstdc++)
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

function(anyps5_add_library_test target)
    cmake_parse_arguments(PARSE_ARGV 1 test "CUSTOM_MAIN" "NAME;LABEL;TIMEOUT;RUNTIME" "SOURCES;LINK;INCLUDES;ARGS;PROPERTIES")
    if(NOT test_LABEL)
        set(test_LABEL integration)
    endif()
    if(NOT test_NAME)
        string(REGEX REPLACE "_tests$" "" test_NAME "${target}")
    endif()
    set(forwarded NAME ${test_NAME} LABEL ${test_LABEL})
    if(test_CUSTOM_MAIN)
        list(APPEND forwarded CUSTOM_MAIN)
    endif()
    if(test_TIMEOUT)
        list(APPEND forwarded TIMEOUT ${test_TIMEOUT})
    endif()
    anyps5_add_test(${target} ${forwarded}
        SOURCES ${test_SOURCES}
        LINK ${test_LINK}
        INCLUDES ${LIBS_INCLUDE_DIR} ${test_INCLUDES}
        ARGS ${test_ARGS}
        PROPERTIES ${test_PROPERTIES}
    )
    configure_windows_unwind(${target})
    if(NOT test_RUNTIME AND test_LINK)
        list(GET test_LINK 0 test_RUNTIME)
    endif()
    if(WIN32 AND test_RUNTIME AND TARGET ${test_RUNTIME})
        set_tests_properties(${test_NAME} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:${test_RUNTIME}>")
    endif()
endfunction()

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc)
add_library(libc SHARED EXCLUDE_FROM_ALL)
set(LIBS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(${LIBC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/prx/libc)
target_include_directories(libc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
configure_windows_unwind(libc)

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
        INSTALL_RPATH "$ORIGIN"
)

set(LIBC_UNPATCHED ${UNPATCHED_DIR}/libc.prx)
set(LIBC_PATCHED ${PATCHED_DIR}/libc.prx)

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
            target_link_libraries(${LIB_NAME} PRIVATE libc)
        else()
            target_link_libraries(${LIB_NAME} PRIVATE libkernel libc)
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
                INSTALL_RPATH "$ORIGIN"
        )

        set(UNPATCHED_FILE ${UNPATCHED_DIR}/${LIB_NAME}.prx)
        set(PATCHED_FILE ${PATCHED_DIR}/${LIB_NAME}.prx)

        add_custom_command(
                OUTPUT ${PATCHED_FILE}
                COMMAND ${CMAKE_COMMAND} -E copy ${UNPATCHED_FILE} ${PATCHED_FILE}
                COMMAND $<TARGET_FILE:nid_patcher> ${LIB_NAME} --preserve-exports "${LIBC_UNPATCHED}" "${PATCHED_FILE}"
                DEPENDS nid_patcher ${LIB_NAME} ${UNPATCHED_FILE} ${LIBC_UNPATCHED}
                COMMENT "Patching NIDs for ${LIB_NAME}"
        )

        list(APPEND PATCHED_OUTPUTS ${PATCHED_FILE})
    endif()
endforeach()

add_custom_target(libs DEPENDS ${PATCHED_OUTPUTS})

if(BUILD_TESTING)
    anyps5_add_library_test(patched_libraries_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/PatchedLibraries.cpp
        ARGS ${PATCHED_DIR}
    )
    add_dependencies(patched_libraries_tests libs)
    if(MINGW)
        target_compile_options(patched_libraries_tests PRIVATE -fasynchronous-unwind-tables)
        target_link_options(patched_libraries_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    anyps5_add_library_test(guest_kernel_sys_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/GuestKernelSys.cpp
        LINK libkernel_sys libkernel libc
    )
    anyps5_add_library_test(guest_offline_imports_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/GuestOfflineImports.cpp
        LINK libSceHttp libSceNpEntitlementAccess libSceRudp libc
    )

    if(UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64"))
        add_subdirectory(tests/register_context)
    endif()

    anyps5_add_library_test(savedata_memory_metadata_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SaveDataMemoryMetadata.cpp
        LINK libSceSaveData.native
    )
    anyps5_add_library_test(savedata_memory_growth_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SaveDataMemoryGrowth.cpp
        LINK libSceSaveData.native
    )
    add_library(host_thread_local_test_library SHARED tests/integration/HostThreadLocalLibrary.cpp)
    set_target_properties(host_thread_local_test_library PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")
    target_include_directories(host_thread_local_test_library PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    anyps5_add_library_test(host_thread_local_tests
        TIMEOUT 15
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/HostThreadLocal.cpp
        LINK host_thread_local_test_library
    )
    anyps5_add_library_test(savedata_core_write_replacement_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SaveDataWriteReplacement.cpp
        LINK libSceSaveData
    )
    anyps5_add_library_test(savedata_native_write_replacement_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SaveDataWriteReplacement.cpp
        LINK libSceSaveData.native
    )
    target_compile_definitions(savedata_native_write_replacement_tests PRIVATE SAVEDATA_NATIVE_BACKEND)
    anyps5_add_library_test(savedata_replace_file_failure_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SaveDataFileWriteFailure.cpp
    )

    if(MINGW)
        foreach(asynchronousUnwindTarget IN ITEMS savedata_memory_metadata_tests savedata_memory_growth_tests
                host_thread_local_test_library host_thread_local_tests savedata_core_write_replacement_tests
                savedata_native_write_replacement_tests savedata_replace_file_failure_tests)
            target_compile_options(${asynchronousUnwindTarget} PRIVATE -fasynchronous-unwind-tables)
        endforeach()
    endif()
endif()
