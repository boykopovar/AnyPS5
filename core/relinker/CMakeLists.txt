option(ANYPS5_RELINKER_CLI "Build the native relinker command line" ON)

add_library(relinker_core STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxElfPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxGuestModuleWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsGuestModuleWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsGuestStartup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/guest/GuestImageReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/guest/GuestModuleBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPePatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsIconResourceBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsLoadImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsTlsBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsTrampolineBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsTlsTemplateBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsRelocationBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsImportBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsDependencyStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsStubEmitter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/macos/MacOsMachOPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/macos/MacOsImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/macos/MacOsGuestModuleWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SegmentFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileIdentity.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/RedZoneFramer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MulxLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionRewriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/StubBodyBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ScalarCompareOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MaskedStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/AndnLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/RelrRelocations.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/SyscallScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CallSiteResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CodeInstructionCollector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/SceCodeExtent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/RelativeJumpTables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictNidFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictReachability.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EhFrameReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/PltCompactor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EntryPointCollector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/ControlFlowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/GotAccessIndex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/RelativeRelocationIndex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/ValidationPolicy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/SysVDynamicSectionBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/CallRegistryWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/patching/GamePatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/pipeline/Conversion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/pipeline/RelinkerPipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/Sha1.cpp
)
target_include_directories(relinker_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/include)
target_include_directories(relinker_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
)

target_compile_features(relinker_core PUBLIC cxx_std_20)
set_target_properties(relinker_core PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileIdentity.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0602)
endif()

if(BUILD_TESTING)
    add_test_executable(andn_lowering_tests ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/AndnLoweringTests.cpp)
    target_link_libraries(andn_lowering_tests PRIVATE relinker_core)
    add_test(NAME andn_lowering COMMAND andn_lowering_tests)
    set_tests_properties(andn_lowering PROPERTIES TIMEOUT 20)

    add_test_executable(masked_store_lowering_tests ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/MaskedStoreLoweringTests.cpp)
    target_link_libraries(masked_store_lowering_tests PRIVATE relinker_core)
    add_test(NAME masked_store_lowering COMMAND masked_store_lowering_tests)
    set_tests_properties(masked_store_lowering PROPERTIES TIMEOUT 20)

    add_test_executable(mulx_lowering_tests ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/MulxLoweringTests.cpp)
    target_link_libraries(mulx_lowering_tests PRIVATE relinker_core)
    add_test(NAME mulx_lowering COMMAND mulx_lowering_tests)
    set_tests_properties(mulx_lowering PROPERTIES TIMEOUT 20)

    add_test_executable(relinker_conversion_tests ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/ConversionTests.cpp)
    target_link_libraries(relinker_conversion_tests PRIVATE relinker_core)
    add_test(NAME relinker_conversion COMMAND relinker_conversion_tests)
    set_tests_properties(relinker_conversion PROPERTIES TIMEOUT 20)

    add_test_executable(native_path_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/NativePathTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp)
    target_include_directories(native_path_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/io/include ${CMAKE_CURRENT_SOURCE_DIR}/domain/include)
    add_test(NAME native_path COMMAND native_path_tests)
    set_tests_properties(native_path PROPERTIES TIMEOUT 20)

    add_test_executable(relinker_game_patch_tests ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/GamePatchTests.cpp)
    target_link_libraries(relinker_game_patch_tests PRIVATE relinker_core)
    add_test(NAME relinker_game_patch COMMAND relinker_game_patch_tests)
    set_tests_properties(relinker_game_patch PROPERTIES TIMEOUT 30)

    add_test_executable(call_registry_writer_tests ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/CallRegistryWriterTests.cpp)
    target_link_libraries(call_registry_writer_tests PRIVATE relinker_core)
    add_test(NAME call_registry_writer COMMAND call_registry_writer_tests)
    set_tests_properties(call_registry_writer PROPERTIES TIMEOUT 20)
endif()

if(ANYPS5_BUILD_FUZZERS)
    add_test_executable(relinker_conversion_fuzzer ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/ConversionFuzzer.cpp)
    target_link_libraries(relinker_conversion_fuzzer PRIVATE relinker_core)
    target_link_options(relinker_conversion_fuzzer PRIVATE -fsanitize=fuzzer)
    if(BUILD_TESTING)
        add_test(NAME relinker_conversion_fuzzer COMMAND relinker_conversion_fuzzer -runs=20000 -seed=1)
        set_tests_properties(relinker_conversion_fuzzer PROPERTIES TIMEOUT 120)
    endif()
endif()

if(ANYPS5_BUILD_WASM)
    target_compile_options(relinker_core PRIVATE -fexceptions)
    add_subdirectory(wasm)
endif()

if(NOT ANYPS5_RELINKER_CLI)
    return()
endif()

add_executable(relinker
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/Autorun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxDesktopEntryWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libkernel/AppMetadata/src/ParamJsonParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/NativeModules.cpp)
target_link_libraries(relinker PRIVATE relinker_core)
target_include_directories(relinker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli/include ${CMAKE_CURRENT_SOURCE_DIR}/../libs)
set_target_properties(relinker PROPERTIES CXX_EXTENSIONS OFF)

find_package(Git QUIET)
set(relinkerRevisionHeader ${CMAKE_CURRENT_BINARY_DIR}/RelinkerRevision.hpp)
add_custom_target(relinker_revision
    COMMAND ${CMAKE_COMMAND} -DGIT=${GIT_EXECUTABLE} -DSOURCE=${CMAKE_SOURCE_DIR} -DOUTPUT=${relinkerRevisionHeader} -P ${CMAKE_CURRENT_SOURCE_DIR}/revision.cmake
    BYPRODUCTS ${relinkerRevisionHeader}
    VERBATIM)
add_dependencies(relinker relinker_revision)
target_include_directories(relinker PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(BUILD_TESTING AND WIN32)
    add_test_executable(windows_guest_export_alias_tests ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/WindowsExportAliasTests.cpp)
    target_link_libraries(windows_guest_export_alias_tests PRIVATE relinker_core)
    add_test(NAME windows_guest_export_aliases COMMAND windows_guest_export_alias_tests)
endif()

if(WIN32)
    add_library(relinker_resources OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/relinker.rc)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/relinker.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/relinker.manifest)
    target_sources(relinker PRIVATE $<TARGET_OBJECTS:relinker_resources>)
    target_compile_options(relinker PRIVATE -municode)
    target_link_options(relinker PRIVATE -municode)
endif()

if(WIN32)
    add_test_executable(windows_dependency_diagnostics_tests EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsDependencyDiagnosticsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsDependencyStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsStubEmitter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsImportBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(windows_dependency_diagnostics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include ${CMAKE_CURRENT_SOURCE_DIR}/domain/include ${CMAKE_CURRENT_SOURCE_DIR}/io/include ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include)
    if(BUILD_TESTING)
        set_target_properties(windows_dependency_diagnostics_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
        add_test(NAME windows_dependency_diagnostics COMMAND windows_dependency_diagnostics_tests)
    endif()

    if(NOT ANYPS5_RELINKER_ONLY)
        add_test_executable(windows_import_identity_tests EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsImportIdentityTests.cpp)
        target_link_libraries(windows_import_identity_tests PRIVATE relinker_core)
        if(BUILD_TESTING)
            set_target_properties(windows_import_identity_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
            add_dependencies(windows_import_identity_tests libs)
            add_test(NAME windows_import_identity COMMAND windows_import_identity_tests "${CMAKE_BINARY_DIR}/core/libs/libs")
        endif()
    endif()

    add_test_executable(amd64_only_windows_tests EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/Amd64OnlyWindowsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsTrampolineBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsLoadImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsStubEmitter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MulxLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionRewriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/StubBodyBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ScalarCompareOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MaskedStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/AndnLowering.cpp
    )
    target_include_directories(amd64_only_windows_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include ${CMAKE_CURRENT_SOURCE_DIR}/domain/include ${CMAKE_CURRENT_SOURCE_DIR}/io/include ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libc/src/specifics/windows)
    if(BUILD_TESTING)
        set_target_properties(amd64_only_windows_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
        add_test(NAME amd64_only_windows COMMAND amd64_only_windows_tests)
    endif()
endif()


if(BUILD_TESTING)
    add_test_executable(elf_reader_bounds_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/ElfReaderBoundsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp)
    target_include_directories(elf_reader_bounds_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include)
    add_test(NAME elf_reader_bounds COMMAND elf_reader_bounds_tests)
    set_tests_properties(elf_reader_bounds PROPERTIES TIMEOUT 20)

    add_test_executable(relr_relocations_tests ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/RelrRelocationTests.cpp)
    target_link_libraries(relr_relocations_tests PRIVATE relinker_core)
    add_test(NAME relr_relocations COMMAND relr_relocations_tests)
    set_tests_properties(relr_relocations PROPERTIES TIMEOUT 20)

    add_test_executable(call_site_index_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/CallSiteIndexTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CallSiteResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_include_directories(call_site_index_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME call_site_index COMMAND call_site_index_tests)
    set_tests_properties(call_site_index PROPERTIES TIMEOUT 20)

    add_test_executable(sdk_mman_syscall_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/SdkMmanSyscallTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/SyscallScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp)
    target_include_directories(sdk_mman_syscall_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include)
    add_test(NAME sdk_mman_syscall COMMAND sdk_mman_syscall_tests)
    set_tests_properties(sdk_mman_syscall PROPERTIES TIMEOUT 20)

    add_test_executable(code_data_scanning_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/CodeDataScanningTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/SyscallScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp)
    target_include_directories(code_data_scanning_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include)
    add_test(NAME code_data_scanning COMMAND code_data_scanning_tests)
    set_tests_properties(code_data_scanning PROPERTIES TIMEOUT 20)

    add_test_executable(runtime_layout_tests ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/RuntimeLayoutTests.cpp)
    target_include_directories(runtime_layout_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli/include)
    add_test(NAME runtime_layout COMMAND runtime_layout_tests)
    set_tests_properties(runtime_layout PROPERTIES TIMEOUT 20)

    add_test_executable(autorun_exit_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/AutorunExitTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/Autorun.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp)
    target_include_directories(autorun_exit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli/include ${CMAKE_CURRENT_SOURCE_DIR}/io/include ${CMAKE_CURRENT_SOURCE_DIR}/domain/include)
    add_test(NAME autorun_exit_status COMMAND autorun_exit_tests)
    set_tests_properties(autorun_exit_status PROPERTIES TIMEOUT 20)
    add_test_executable(rewrite_boundaries_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/RewriteBoundaryTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionRewriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_include_directories(rewrite_boundaries_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME rewrite_boundaries COMMAND rewrite_boundaries_tests)
    set_tests_properties(rewrite_boundaries PROPERTIES TIMEOUT 20)

    add_test_executable(red_zone_framer_tests ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/RedZoneFramerTests.cpp)
    target_link_libraries(red_zone_framer_tests PRIVATE relinker_core)
    add_test(NAME red_zone_framer COMMAND red_zone_framer_tests)
    if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32) AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_test_executable(red_zone_framer_execution_tests ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/RedZoneFramerExecutionTests.cpp)
        target_link_libraries(red_zone_framer_execution_tests PRIVATE relinker_core)
        add_test(NAME red_zone_framer_execution COMMAND red_zone_framer_execution_tests)
        set_tests_properties(red_zone_framer_execution PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    endif()

    add_test_executable(cli_args_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/CliArgsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp)
    target_include_directories(cli_args_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
    )
    add_test(NAME cli_args COMMAND cli_args_tests)
    add_test_executable(program_layout_ranges_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/ProgramLayoutRangeTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
    )
    target_include_directories(program_layout_ranges_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME program_layout_ranges COMMAND program_layout_ranges_tests)
    set_tests_properties(program_layout_ranges PROPERTIES TIMEOUT 20)

    add_test_executable(buffer_bounds_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/BufferBoundsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(buffer_bounds_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/io/include)
    add_test(NAME buffer_bounds COMMAND buffer_bounds_tests)

    add_test_executable(file_identity_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/FileIdentityTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileIdentity.cpp
    )
    target_include_directories(file_identity_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/io/include)
    add_test(NAME file_identity COMMAND file_identity_tests)
    set_tests_properties(file_identity PROPERTIES TIMEOUT 20)

    add_test_executable(file_reader_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/FileReaderTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
    )
    target_include_directories(file_reader_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
    )
    add_test(NAME file_reader_prefix COMMAND file_reader_tests)
    set_tests_properties(file_reader_prefix PROPERTIES TIMEOUT 20)

    add_test_executable(param_json_utf8_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/tests/ParamJsonParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libkernel/AppMetadata/src/ParamJsonParser.cpp)
    target_include_directories(param_json_utf8_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libs)
    add_test(NAME param_json_utf8 COMMAND param_json_utf8_tests)
    set_tests_properties(param_json_utf8 PROPERTIES TIMEOUT 20)

    if(WIN32)
        add_test_executable(windows_file_writer_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/WindowsFileWriterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        )
        target_include_directories(windows_file_writer_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        )
        add_test(NAME windows_file_writer_binary COMMAND windows_file_writer_tests binary)
        add_test(NAME windows_file_writer_text COMMAND windows_file_writer_tests text)
        set_tests_properties(windows_file_writer_binary windows_file_writer_text PROPERTIES TIMEOUT 20)
    endif()

    if(UNIX)
        add_test_executable(symlink_file_writer_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/SymlinkFileWriterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        )
        target_include_directories(symlink_file_writer_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        )
        add_test(NAME symlink_file_writer_binary COMMAND symlink_file_writer_tests binary)
        add_test(NAME symlink_file_writer_text COMMAND symlink_file_writer_tests text)
        set_tests_properties(symlink_file_writer_binary symlink_file_writer_text PROPERTIES TIMEOUT 20)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(file_writer_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/FileWriterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        )
        target_include_directories(file_writer_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        )
        add_test(NAME file_writer_binary COMMAND file_writer_tests binary)
        add_test(NAME file_writer_text COMMAND file_writer_tests text)

        add_test_executable(linux_desktop_entry_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/LinuxDesktopEntryTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxDesktopEntryWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libkernel/AppMetadata/src/ParamJsonParser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/NativePath.cpp
        )
        target_include_directories(linux_desktop_entry_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../libs
        )
        add_test(NAME linux_desktop_entry COMMAND linux_desktop_entry_tests)
    endif()

    add_test_executable(section_header_ranges_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/SectionRangeTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(section_header_ranges_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME section_header_ranges COMMAND section_header_ranges_tests)
    set_tests_properties(section_header_ranges PROPERTIES TIMEOUT 20)

    add_test_executable(linux_load_pages_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/LinuxLoadPageTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxElfPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SegmentFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/StubBodyBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/Sha1.cpp
    )
    target_include_directories(linux_load_pages_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/include
    )
    add_test(NAME linux_load_pages COMMAND linux_load_pages_tests)
    set_tests_properties(linux_load_pages PROPERTIES TIMEOUT 20)

    add_test_executable(amd64_only_converter_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/Amd64OnlyConverterTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MulxLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionRewriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/DecodedInstruction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sse4aLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/StubBodyBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha256Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Operands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/Sha1Lowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ClzeroLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ReciprocalLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/WideStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/ScalarCompareOperands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/MaskedStoreLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/AndnLowering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxElfPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SegmentFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/src/Sha1.cpp
    )
    target_include_directories(amd64_only_converter_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/nid/include
    )
    if(APPLE AND (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")))
        target_link_options(amd64_only_converter_tests PRIVATE "LINKER:-pagezero_size,0x10000")
    endif()
    add_test(NAME amd64_only_converter COMMAND amd64_only_converter_tests)
    add_test_executable(call_site_resolver_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/CallSiteResolverTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CallSiteResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/InstructionScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_include_directories(call_site_resolver_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
    )
    add_test(NAME call_site_resolver COMMAND call_site_resolver_tests)
    add_test_executable(decoder_length_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/DecoderLengthTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_include_directories(decoder_length_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
    )
    add_test(NAME decoder_length COMMAND decoder_length_tests)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME cli_help COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/test_help.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_fixed_base COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_fixed_base.py $<TARGET_FILE:relinker>)
        set_tests_properties(windows_fixed_base PROPERTIES TIMEOUT 20)
        add_test(NAME linux_gnu_segments COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_gnu_segments.py $<TARGET_FILE:relinker>)
        set_tests_properties(linux_gnu_segments PROPERTIES TIMEOUT 20)
        add_test(NAME linux_tls_alignment COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_tls_alignment.py $<TARGET_FILE:relinker>)
        set_tests_properties(linux_tls_alignment PROPERTIES TIMEOUT 20)
        add_test(NAME elf_offsets COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_elf_offsets.py $<TARGET_FILE:relinker>)
        add_test(NAME conversion_diagnostics COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_conversion_diagnostics.py $<TARGET_FILE:relinker>)
        set_tests_properties(conversion_diagnostics PROPERTIES TIMEOUT 20)
        add_test(NAME dynamic_segment COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_dynamic_segment.py $<TARGET_FILE:relinker>)
        add_test(NAME rela_size COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_rela_size.py $<TARGET_FILE:relinker>)
        add_test(NAME none_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_none_relocations.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_none_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_none_relocations.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_none_relocations PROPERTIES TIMEOUT 20)
        add_test(NAME interpreter_in_code COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_interpreter_in_code.py $<TARGET_FILE:relinker>)
        set_tests_properties(interpreter_in_code PROPERTIES TIMEOUT 20)
        add_test(NAME guest_relative_section_symbols COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_relative_section_symbols.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_relative_section_symbols PROPERTIES TIMEOUT 20)
        add_test(NAME overlapping_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_overlapping_relocations.py $<TARGET_FILE:relinker>)
        set_tests_properties(overlapping_relocations PROPERTIES TIMEOUT 30)
        add_test(NAME duplicate_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_duplicate_relocations.py $<TARGET_FILE:relinker>)
        set_tests_properties(duplicate_relocations PROPERTIES TIMEOUT 30)
        add_test(NAME guest_own_tls_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_own_tls_relocations.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_own_tls_relocations PROPERTIES TIMEOUT 30)
        add_test(NAME optional_plt COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_optional_plt.py $<TARGET_FILE:relinker>)
        add_test(NAME optional_rela COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_optional_rela.py $<TARGET_FILE:relinker>)
        add_test(NAME relr_conversion COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_relr_relocations.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_relr_conversion COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_relr_relocations.py $<TARGET_FILE:relinker> guest)
        set_tests_properties(relr_conversion guest_relr_conversion PROPERTIES TIMEOUT 20)
        add_test(NAME empty_tls COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_empty_tls.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_tls_zero_fill COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_tls_zero_fill.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_tls_protected_entries COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_tls_protected_entries.py $<TARGET_FILE:relinker>)
        add_test(NAME gs_trap_entry_roots COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_gs_trap_entry_roots.py $<TARGET_FILE:relinker>)
        set_tests_properties(gs_trap_entry_roots PROPERTIES TIMEOUT 30)
        set(tlsCoverageGroups metadata padding register_loads displacement_bounds alu_execution register_address_loads moved_branch rejected)
        foreach(register IN ITEMS 0 1 2 3 5 6 7 8 9 10 11 12 13 14 15)
            list(APPEND tlsCoverageGroups displacement_r${register})
        endforeach()
        foreach(opcode IN ITEMS add or adc sbb and sub xor cmp)
            list(APPEND tlsCoverageGroups alu_${opcode})
        endforeach()
        list(APPEND tlsCoverageGroups gs_traps gs_traps_implicit gs_non_memory)
        foreach(group IN LISTS tlsCoverageGroups)
            add_test(NAME tls_function_coverage_${group} COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_tls_function_coverage.py $<TARGET_FILE:relinker> ${group})
            set_tests_properties(tls_function_coverage_${group} PROPERTIES TIMEOUT 30 LABELS tls_function_coverage)
        endforeach()
        add_test(NAME tls_diagnostics COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_tls_diagnostics.py $<TARGET_FILE:relinker>)
        add_test(NAME ignored_string_segments COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_ignored_string_segments.py $<TARGET_FILE:relinker>)
        set_tests_properties(ignored_string_segments PROPERTIES TIMEOUT 30)
        add_test(NAME call_zero_padding COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_call_zero_padding.py $<TARGET_FILE:relinker>)
        set_tests_properties(call_zero_padding PROPERTIES TIMEOUT 60)
        add_test(NAME macos_tls_alu COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_tls_alu.py $<TARGET_FILE:relinker>)
        set_tests_properties(macos_tls_alu PROPERTIES TIMEOUT 60)
        add_test(NAME linux_load_alignment COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_load_alignment.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_load_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_load_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_eh_frame_header COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_eh_frame_header.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_dynamic_strings COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_dynamic_strings.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_runpath COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_runpath.py $<TARGET_FILE:relinker>)
        add_test(NAME executable_segments COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executable_segments.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_executable_exports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_executable_exports.py $<TARGET_FILE:relinker>)
        add_test(NAME executable_tls_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executable_tls_relocations.py $<TARGET_FILE:relinker>)
        add_test(NAME executable_export_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executable_export_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME executable_export_attributes COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executable_export_attributes.py $<TARGET_FILE:relinker>)
        add_test(NAME macos_empty_tls COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_empty_tls.py $<TARGET_FILE:relinker>)
        set_tests_properties(macos_empty_tls PROPERTIES TIMEOUT 60)
        add_test(NAME linux_entry_argv COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_entry_argv.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_fixed_base COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_fixed_base.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_program_header_order COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_program_header_order.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_input_program_headers COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_input_program_headers.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_gui COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_gui.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_working_directory COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_working_directory.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_runpath COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_runpath.py $<TARGET_FILE:relinker>)
        set_tests_properties(windows_runpath PROPERTIES TIMEOUT 30)
        add_test(NAME windows_trace_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_trace_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_trace_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_trace_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME macos_trace_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_trace_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_address_space COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_address_space.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_tls_thread_pointer COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_tls_thread_pointer.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_bss COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_bss.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_lazy_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_lazy_imports.py $<TARGET_FILE:relinker>)
        set_tests_properties(windows_lazy_imports PROPERTIES TIMEOUT 30)
        add_test(NAME windows_sparse_load COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_sparse_load.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_icon COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_icon.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_desktop_entry_output COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_desktop_entry_output.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_intel_trampolines COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_intel_trampolines.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_syscall_trap COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_syscall_trap.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_tls COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_tls.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_tls_export_references COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_tls_export_references.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_tls_export_references PROPERTIES TIMEOUT 30)
        add_test(NAME guest_needed_modules COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_needed_modules.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_module_dir COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_dir.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_module_directories COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_directories.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_trailing_note COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_trailing_note.py $<TARGET_FILE:relinker>)
        add_test(NAME relinker_executables COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executables.py $<TARGET_FILE:relinker>)
        add_test(NAME game_patches COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_game_patches.py $<TARGET_FILE:relinker>)
        add_test(NAME annotate_guest_module COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_annotate_guest_module.py $<TARGET_FILE:relinker>)
        if(Python3_VERSION VERSION_GREATER_EQUAL 3.11)
            add_test(NAME patch_advisor_unit COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/tests/test_suggest_patches.py)
            add_test(NAME patch_advisor_integration COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_patch_advisor.py $<TARGET_FILE:relinker>)
            set_tests_properties(patch_advisor_integration PROPERTIES TIMEOUT 40)
        endif()
        add_test(NAME sce_module_path COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_sce_module_path.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_guest_stack COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_guest_stack.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_symbol_names COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_symbol_names.py $<TARGET_FILE:relinker>)
        add_test(NAME import_symbol_types COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_import_symbol_types.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_registry COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_registry.py $<TARGET_FILE:relinker>)
        add_test(NAME needed_libraries COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_needed_libraries.py $<TARGET_FILE:relinker>)
        set_tests_properties(needed_libraries PROPERTIES TIMEOUT 30)
        add_test(NAME macos_output COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_output.py $<TARGET_FILE:relinker>)
        add_test(NAME macos_tls_overlap COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_tls_overlap.py $<TARGET_FILE:relinker>)
        set_tests_properties(macos_tls_overlap PROPERTIES TIMEOUT 60)
        add_test(NAME macos_tls_exports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_tls_exports.py $<TARGET_FILE:relinker>)
        set_tests_properties(macos_tls_exports PROPERTIES TIMEOUT 30)
        add_test(NAME macos_gs_traps COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_gs_traps.py $<TARGET_FILE:relinker>)
        set_tests_properties(macos_gs_traps PROPERTIES TIMEOUT 60)
        add_test(NAME macos_fixed_base COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_fixed_base.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_host_libc COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_host_libc.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_named_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_named_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_named_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_named_imports.py $<TARGET_FILE:relinker>)
        add_library(guest_module_args_host SHARED elfpatcher/tests/GuestModuleArgsHost.cpp)
        target_include_directories(guest_module_args_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libs)
        set_target_properties(guest_module_args_host PROPERTIES
            PREFIX "" SUFFIX ".prx" WINDOWS_EXPORT_ALL_SYMBOLS ON)
        add_test(NAME guest_module_args COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_args.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        add_test(NAME guest_deferred_dependencies COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_deferred_dependencies.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        add_test(NAME guest_absolute_symbols COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_absolute_symbols.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_module_identity COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_identity.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_unused_import_dependencies COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_unused_import_dependencies.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        set_property(TEST guest_unused_import_dependencies APPEND PROPERTY LABELS relinker)
        add_asan_test_options(guest_unused_import_dependencies verify_asan_link_order=0)
        add_test(NAME guest_export_lookup COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_export_lookup.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_export_lookup PROPERTIES TIMEOUT 30)
        add_test(NAME guest_module_fini COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_fini.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        add_asan_test_options(guest_module_args verify_asan_link_order=0)
        add_asan_test_options(guest_deferred_dependencies verify_asan_link_order=0)
        add_asan_test_options(guest_module_fini verify_asan_link_order=0)
        add_asan_test_options(guest_unused_import_dependencies verify_asan_link_order=0)
        add_test(NAME guest_needed_nested_modules COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_needed_nested_modules.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_zero_initializer COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_zero_initializer.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        set_tests_properties(guest_zero_initializer PROPERTIES TIMEOUT 30)
        add_asan_test_options(guest_zero_initializer verify_asan_link_order=0)
        add_test(NAME linux_guest_named_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_guest_named_imports.py
            $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
        add_asan_test_options(linux_guest_named_imports verify_asan_link_order=0)
        set(windows_stdio_host_arguments)
        if(WIN32)
            if(MINGW)
                get_filename_component(testCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
                set(mingwDlltoolExecutable "${testCompilerDirectory}/dlltool.exe")
                if(NOT EXISTS "${mingwDlltoolExecutable}")
                    message(FATAL_ERROR "Cannot locate MinGW DLL tool: ${mingwDlltoolExecutable}")
                endif()
                set(ucrtImportLibrary "${CMAKE_CURRENT_BINARY_DIR}/windows_stdio_ucrt_import.a")
                add_custom_command(OUTPUT "${ucrtImportLibrary}"
                    COMMAND "${mingwDlltoolExecutable}" -d "${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsUcrt.def"
                        -l "${ucrtImportLibrary}" -D ucrtbase.dll --temp-prefix windows_stdio_ucrt_import
                    DEPENDS elfpatcher/tests/WindowsUcrt.def
                    VERBATIM)
                add_custom_target(windows_stdio_ucrt_import DEPENDS "${ucrtImportLibrary}")
                add_library(windows_stdio_host_ucrt SHARED elfpatcher/tests/WindowsUcrtStdioHost.cpp)
                add_dependencies(windows_stdio_host_ucrt windows_stdio_ucrt_import)
                target_link_libraries(windows_stdio_host_ucrt PRIVATE "${ucrtImportLibrary}")
                target_compile_options(windows_stdio_host_ucrt PRIVATE -fno-builtin)
                target_link_options(windows_stdio_host_ucrt PRIVATE -nostdlib -Wl,--no-undefined -Wl,--entry,0)
                set_target_properties(windows_stdio_host_ucrt PROPERTIES PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS ON)
                list(APPEND windows_stdio_host_arguments $<TARGET_FILE:windows_stdio_host_ucrt>)

                set(msvcrtImportLibrary "${CMAKE_CURRENT_BINARY_DIR}/windows_stdio_msvcrt_import.a")
                add_custom_command(OUTPUT "${msvcrtImportLibrary}"
                    COMMAND "${mingwDlltoolExecutable}" -d "${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsMsvcrt.def"
                        -l "${msvcrtImportLibrary}" -D msvcrt.dll --temp-prefix windows_stdio_msvcrt_import
                    DEPENDS elfpatcher/tests/WindowsMsvcrt.def
                    VERBATIM)
                add_custom_target(windows_stdio_msvcrt_import DEPENDS "${msvcrtImportLibrary}")
                add_library(windows_stdio_host_msvcrt SHARED elfpatcher/tests/WindowsMsvcrtStdioHost.cpp)
                add_dependencies(windows_stdio_host_msvcrt windows_stdio_msvcrt_import)
                target_link_libraries(windows_stdio_host_msvcrt PRIVATE "${msvcrtImportLibrary}")
                target_compile_options(windows_stdio_host_msvcrt PRIVATE -fno-builtin)
                target_link_options(windows_stdio_host_msvcrt PRIVATE -nostdlib -Wl,--no-undefined -Wl,--entry,0)
                set_target_properties(windows_stdio_host_msvcrt PROPERTIES PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS ON)
                list(APPEND windows_stdio_host_arguments $<TARGET_FILE:windows_stdio_host_msvcrt>)
            else()
                add_library(windows_stdio_host SHARED elfpatcher/tests/WindowsStdioHost.cpp)
                set_target_properties(windows_stdio_host PROPERTIES PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS ON)
                list(APPEND windows_stdio_host_arguments $<TARGET_FILE:windows_stdio_host>)
            endif()
        endif()
        add_test(NAME windows_import_modules COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_import_modules.py
            $<TARGET_FILE:relinker> ${windows_stdio_host_arguments})
        if(WIN32)
            foreach(kernelVariant IN ITEMS sys web)
                add_test(NAME windows_kernel_variant_${kernelVariant} COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_kernel_variant_imports.py
                    $<TARGET_FILE:relinker> libkernel_${kernelVariant}.prx)
                set_tests_properties(windows_kernel_variant_${kernelVariant} PROPERTIES TIMEOUT 30)
            endforeach()
        endif()
        add_test(NAME elf_header COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_elf_header.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_process_parameters COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_process_parameters.py $<TARGET_FILE:relinker>)
        add_test(NAME weak_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_weak_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_guest_weak_imports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_guest_weak_imports.py $<TARGET_FILE:relinker>)
        add_test(NAME input_magic COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_input_magic.py $<TARGET_FILE:relinker>)
        add_asan_test_options(input_magic max_allocation_size_mb=512)
        add_test(NAME input_preservation COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_input_preservation.py $<TARGET_FILE:relinker>)
        add_test(NAME output_collisions COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_output_collisions.py $<TARGET_FILE:relinker>)
        add_test(NAME secondary_output_collisions COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_secondary_output_collisions.py $<TARGET_FILE:relinker>)
        add_test(NAME input_file COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_input_file.py $<TARGET_FILE:relinker>)
        add_test(NAME null_symbol_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_null_symbol_relocations.py $<TARGET_FILE:relinker>)
        foreach(symbolInput IN ITEMS executable guest)
            foreach(symbolRelocation IN ITEMS 1 6 7)
                add_test(NAME restricted_undefined_symbols_${symbolInput}_${symbolRelocation} COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_restricted_undefined_symbols.py $<TARGET_FILE:relinker> ${symbolInput} ${symbolRelocation})
                set_tests_properties(restricted_undefined_symbols_${symbolInput}_${symbolRelocation} PROPERTIES TIMEOUT 20)
            endforeach()
        endforeach()
        add_test(NAME input_nonascii COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_input_nonascii.py $<TARGET_FILE:relinker>)
        add_test(NAME long_paths COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_long_paths.py $<TARGET_FILE:relinker>)
        set_tests_properties(long_paths PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME string_table_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_string_table_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME sce_code_extent COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_sce_code_extent.py $<TARGET_FILE:relinker>)
        add_test(NAME sce_relro COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_sce_relro.py $<TARGET_FILE:relinker>)
        add_test(NAME sce_module_code_extent COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_sce_module_code_extent.py $<TARGET_FILE:relinker>)
        add_test(NAME sce_code_gaps COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_sce_code_gaps.py $<TARGET_FILE:relinker>)
        add_test(NAME call_registry COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_call_registry.py $<TARGET_FILE:relinker>)
        add_test(NAME jmprel_table_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_jmprel_table_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME copy_relocations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_copy_relocations.py $<TARGET_FILE:relinker>)
        if(WIN32)
            add_test(NAME windows_diagnostic_fixture_isolation COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/test_windows_diagnostic_fixture_isolation.py $<TARGET_FILE:windows_dependency_diagnostics_tests>)
            set_tests_properties(windows_diagnostic_fixture_isolation PROPERTIES TIMEOUT 30)
        endif()
        add_test(NAME entry_point_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_entry_point_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME init_array_roots COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_init_array_roots.py $<TARGET_FILE:relinker>)
        add_test(NAME gnu_hash_analysis COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_gnu_hash_analysis.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_gnu_hash COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_gnu_hash.py $<TARGET_FILE:relinker>)
        set_tests_properties(guest_gnu_hash PROPERTIES TIMEOUT 20)
        add_test(NAME mulx_conversion COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_mulx_conversion.py $<TARGET_FILE:relinker>)
        set_tests_properties(mulx_conversion PROPERTIES TIMEOUT 20)
        add_test(NAME to_rosetta COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_to_rosetta.py $<TARGET_FILE:relinker>)
        add_test(NAME syscall_number COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_syscall_number.py $<TARGET_FILE:relinker>)
        add_test(NAME unused_filter_indirect COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_unused_filter_indirect.py $<TARGET_FILE:relinker>)
        add_test(NAME unused_filter_fallthrough_padding COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_unused_filter_fallthrough_padding.py $<TARGET_FILE:relinker>)
    else()
        message(STATUS "Python3 interpreter not found; skipping Python relinker tests")
    endif()
    add_test_executable(unwind_record_bounds_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/UnwindRecordBoundsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EhFrameReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(unwind_record_bounds_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME unwind_record_bounds COMMAND unwind_record_bounds_tests)
    set_tests_properties(unwind_record_bounds PROPERTIES TIMEOUT 20)

    add_test_executable(windows_icon_resource_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsIconResourceTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsIconResourceBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(windows_icon_resource_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME windows_icon_resource COMMAND windows_icon_resource_tests)

    add_test_executable(windows_trampoline_builder_tests ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/WindowsTrampolineBuilderTests.cpp)
    target_link_libraries(windows_trampoline_builder_tests PRIVATE relinker_core)
    add_test(NAME windows_trampoline_builder COMMAND windows_trampoline_builder_tests)

    add_test_executable(entry_stub_tests ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/EntryStubTests.cpp)
    target_link_libraries(entry_stub_tests PRIVATE relinker_core)
    add_test(NAME entry_stub COMMAND entry_stub_tests)

    add_test_executable(control_flow_graph_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/ControlFlowGraphTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/RelativeJumpTables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/ControlFlowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictReachability.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_include_directories(control_flow_graph_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    )
    add_test(NAME control_flow_graph COMMAND control_flow_graph_tests)
    set_tests_properties(control_flow_graph PROPERTIES TIMEOUT 20)

    add_test_executable(strict_nid_filter_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/StrictNidFilterTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictNidFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictReachability.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EhFrameReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/PltCompactor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/SysVDynamicSectionBuilder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
    )
    target_sources(strict_nid_filter_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/RelrRelocations.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/ControlFlowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EntryPointCollector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/GotAccessIndex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/RelativeRelocationIndex.cpp
    )
    target_include_directories(strict_nid_filter_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    )
    add_test(NAME strict_nid_filter COMMAND strict_nid_filter_tests)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_test_executable(to_intel_linux_stubs_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/ToIntelLinuxStubsTests.cpp
        )
        add_test(NAME to_intel_linux_stubs COMMAND to_intel_linux_stubs_tests $<TARGET_FILE:relinker>)

        add_test_executable(rela_bounds_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/RelaBoundsTests.cpp
        )
        add_test(NAME rela_bounds COMMAND rela_bounds_tests $<TARGET_FILE:relinker>)

        add_test_executable(syscall_scanner_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/SyscallScannerTests.cpp
        )
        add_test(NAME syscall_scanner COMMAND syscall_scanner_tests $<TARGET_FILE:relinker>)
        add_test_executable(guest_intel_trampolines_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/GuestIntelTrampolinesTests.cpp
        )
        add_test(NAME guest_intel_trampolines_execution COMMAND guest_intel_trampolines_tests $<TARGET_FILE:relinker>)

        add_test_executable(dynamic_table_bounds_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/DynamicTableBoundsTests.cpp
        )
        add_test(NAME dynamic_table_bounds COMMAND dynamic_table_bounds_tests $<TARGET_FILE:relinker>)

        add_test_executable(reloc_table_bounds_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/RelocTableBoundsTests.cpp
        )
        add_test(NAME reloc_table_bounds COMMAND reloc_table_bounds_tests $<TARGET_FILE:relinker>)
    endif()
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME macos_guest_weak_exports COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_guest_weak_exports.py $<TARGET_FILE:relinker>)
    set_tests_properties(macos_guest_weak_exports PROPERTIES TIMEOUT 30 LABELS relinker)
    add_test(NAME macos_executable_exports COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_executable_exports.py $<TARGET_FILE:relinker>)
    set_tests_properties(macos_executable_exports PROPERTIES TIMEOUT 30 LABELS relinker)
    add_test(NAME macos_guest_weak_imports COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_macos_guest_weak_imports.py $<TARGET_FILE:relinker>)
    set_property(TEST macos_guest_weak_imports APPEND PROPERTY LABELS relinker)
endif()

if(BUILD_TESTING)
    set(relinkerTestTemp ${CMAKE_CURRENT_BINARY_DIR}/test-temp)
    file(MAKE_DIRECTORY ${relinkerTestTemp})
    get_property(relinkerTests DIRECTORY PROPERTY TESTS)
    set_property(TEST ${relinkerTests} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "TMPDIR=set:${relinkerTestTemp};TEMP=set:${relinkerTestTemp};TMP=set:${relinkerTestTemp};PYTHONUTF8=set:1")
endif()
