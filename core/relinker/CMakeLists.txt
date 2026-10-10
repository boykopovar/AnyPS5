add_executable(relinker
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/Autorun.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/SyscallScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CallSiteResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/CodeInstructionCollector.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/pipeline/RelinkerPipeline.cpp
)
target_include_directories(relinker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/include
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
)

if(BUILD_TESTING)
    if(WIN32)
        anyps5_add_test(windows_dependency_diagnostics_tests
            NAME windows_dependency_diagnostics
            LABEL unit
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/unit/WindowsDependencyDiagnosticsTests.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsDependencyStubBuilder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsStubEmitter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsImportBuilder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeWriter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
                ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
                ${CMAKE_CURRENT_SOURCE_DIR}/io/include
                ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        )
        anyps5_add_test(amd64_only_windows_tests
            LABEL unit
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/unit/Amd64OnlyWindowsTests.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsTrampolineBuilder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsLoadImage.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsStubEmitter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeWriter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
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
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
                ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
                ${CMAKE_CURRENT_SOURCE_DIR}/io/include
                ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
                ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libc/src/specifics/windows
        )
    endif()

    anyps5_add_test(autorun_exit_tests
        CUSTOM_MAIN
        NAME autorun_exit_status
        LABEL integration
        TIMEOUT 20
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/integration/AutorunExitTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/Autorun.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/include
    )
    anyps5_add_test(cli_args_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/unit/CliArgsTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/include
    )
    anyps5_add_test(buffer_bounds_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/unit/BufferBoundsTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteReader.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        anyps5_add_test(file_writer_tests
            NAME file_writer
            LABEL integration
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/integration/FileWriterTests.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
                ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        )
    endif()
    anyps5_add_test(amd64_only_converter_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/unit/Amd64OnlyConverterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/Amd64OnlyConverter.cpp
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
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxElfPatcher.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SegmentFilter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    anyps5_add_test(windows_icon_resource_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/unit/WindowsIconResourceTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsIconResourceBuilder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/windows/WindowsPeFormat.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    anyps5_add_test(entry_stub_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/unit/EntryStubTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
    )
    anyps5_add_test(strict_nid_filter_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/unit/StrictNidFilterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictNidFilter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/StrictReachability.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EhFrameReader.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/PltCompactor.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/SysVDynamicSectionBuilder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/x86/X64InstructionDecoder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/ControlFlowGraph.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/EntryPointCollector.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/GotAccessIndex.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/analysis/UnusedNidFilter/RelativeRelocationIndex.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    )

    foreach(relinkerTest IN ITEMS
            "elf_offsets|test_elf_offsets.py"
            "dynamic_segment|test_dynamic_segment.py"
            "rela_size|test_rela_size.py"
            "optional_plt|test_optional_plt.py"
            "empty_tls|test_empty_tls.py"
            "tls_function_coverage|test_tls_function_coverage.py"
            "tls_diagnostics|test_tls_diagnostics.py"
            "linux_load_alignment|test_linux_load_alignment.py"
            "linux_eh_frame_header|test_linux_eh_frame_header.py"
            "linux_dynamic_strings|test_linux_dynamic_strings.py"
            "guest_runpath|test_guest_runpath.py"
            "executable_segments|test_executable_segments.py"
            "linux_entry_argv|test_linux_entry_argv.py"
            "linux_program_header_order|test_linux_program_header_order.py"
            "windows_gui|test_windows_gui.py"
            "windows_address_space|test_windows_address_space.py"
            "windows_icon|test_windows_icon.py"
            "guest_intel_trampolines|test_guest_intel_trampolines.py"
            "guest_tls|test_guest_tls.py"
            "guest_needed_modules|test_guest_needed_modules.py"
            "guest_module_directories|test_guest_module_directories.py"
            "linux_guest_stack|test_linux_guest_stack.py"
            "guest_symbol_names|test_guest_symbol_names.py"
            "guest_registry|test_guest_registry.py"
            "macos_output|test_macos_output.py"
            "guest_host_libc|test_guest_host_libc.py"
            "guest_absolute_symbols|test_guest_absolute_symbols.py"
            "guest_module_identity|test_guest_module_identity.py"
            "windows_import_modules|test_windows_import_modules.py"
            "elf_header|test_elf_header.py"
            "input_magic|test_input_magic.py"
            "input_preservation|test_input_preservation.py"
            "string_table_bounds|test_string_table_bounds.py"
            "call_registry|test_call_registry.py"
            "jmprel_table_bounds|test_jmprel_table_bounds.py"
            )
        string(REPLACE "|" ";" relinkerTest "${relinkerTest}")
        list(GET relinkerTest 0 relinkerTestName)
        list(GET relinkerTest 1 relinkerTestScript)
        anyps5_add_python_test(${relinkerTestName} LABEL e2e
            SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/${relinkerTestScript}
            ARGS $<TARGET_FILE:relinker>)
    endforeach()

    add_library(guest_module_args_host SHARED elfpatcher/tests/e2e/GuestModuleArgsHost.cpp)
    target_include_directories(guest_module_args_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libs)
    set_target_properties(guest_module_args_host PROPERTIES
        PREFIX "" SUFFIX ".prx" WINDOWS_EXPORT_ALL_SYMBOLS ON)
    anyps5_add_python_test(guest_module_args LABEL e2e
        SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/test_guest_module_args.py
        ARGS $<TARGET_FILE:relinker> $<TARGET_FILE:guest_module_args_host>)
    if(WIN32)
        anyps5_add_python_test(windows_diagnostic_fixture_isolation LABEL e2e TIMEOUT 30
            SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/e2e/test_windows_diagnostic_fixture_isolation.py
            ARGS $<TARGET_FILE:windows_dependency_diagnostics_tests>)
    endif()
    if(NOT Python3_Interpreter_FOUND)
        message(STATUS "Python3 interpreter not found; skipping Python relinker tests")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        anyps5_add_test(to_intel_linux_stubs_tests
            LABEL e2e
            ARGS $<TARGET_FILE:relinker>
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/ToIntelLinuxStubsTests.cpp
        )
        anyps5_add_test(rela_bounds_tests
            LABEL e2e
            ARGS $<TARGET_FILE:relinker>
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/RelaBoundsTests.cpp
        )
        anyps5_add_test(guest_intel_trampolines_tests
            NAME guest_intel_trampolines_execution
            LABEL e2e
            ARGS $<TARGET_FILE:relinker>
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/GuestIntelTrampolinesTests.cpp
        )
        anyps5_add_test(dynamic_table_bounds_tests
            LABEL e2e
            ARGS $<TARGET_FILE:relinker>
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/DynamicTableBoundsTests.cpp
        )
        anyps5_add_test(reloc_table_bounds_tests
            LABEL e2e
            ARGS $<TARGET_FILE:relinker>
            SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/e2e/RelocTableBoundsTests.cpp
        )
    endif()

    anyps5_add_test(elf_reader_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/unit/ElfReaderTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    )
    anyps5_add_test(guest_image_reader_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/unit/GuestImageReaderTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/guest/GuestImageReader.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
            ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    )
endif()
