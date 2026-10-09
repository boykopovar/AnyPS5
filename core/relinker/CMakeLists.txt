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

    add_test_executable(amd64_only_windows_tests EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/tests/Amd64OnlyWindowsTests.cpp
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
    )
    target_include_directories(amd64_only_windows_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include ${CMAKE_CURRENT_SOURCE_DIR}/domain/include ${CMAKE_CURRENT_SOURCE_DIR}/io/include ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include ${CMAKE_CURRENT_SOURCE_DIR}/../libs/prx/libc/src/specifics/windows)
    if(BUILD_TESTING)
        set_target_properties(amd64_only_windows_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
        add_test(NAME amd64_only_windows COMMAND amd64_only_windows_tests)
    endif()
endif()


if(BUILD_TESTING)
    add_test_executable(autorun_exit_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/AutorunExitTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/Autorun.cpp)
    target_include_directories(autorun_exit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli/include)
    add_test(NAME autorun_exit_status COMMAND autorun_exit_tests)
    set_tests_properties(autorun_exit_status PROPERTIES TIMEOUT 20)
    add_test_executable(cli_args_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/CliArgsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp)
    target_include_directories(cli_args_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli/include)
    add_test(NAME cli_args COMMAND cli_args_tests)
    add_test_executable(buffer_bounds_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/BufferBoundsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    )
    target_include_directories(buffer_bounds_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/io/include)
    add_test(NAME buffer_bounds COMMAND buffer_bounds_tests)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(file_writer_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/FileWriterTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
        )
        target_include_directories(file_writer_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
            ${CMAKE_CURRENT_SOURCE_DIR}/io/include
        )
        add_test(NAME file_writer_binary COMMAND file_writer_tests binary)
        add_test(NAME file_writer_text COMMAND file_writer_tests text)
    endif()

    add_test_executable(amd64_only_converter_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/tests/Amd64OnlyConverterTests.cpp
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
    )
    target_include_directories(amd64_only_converter_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    )
    add_test(NAME amd64_only_converter COMMAND amd64_only_converter_tests)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME elf_offsets COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_elf_offsets.py $<TARGET_FILE:relinker>)
        add_test(NAME dynamic_segment COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_dynamic_segment.py $<TARGET_FILE:relinker>)
        add_test(NAME rela_size COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_rela_size.py $<TARGET_FILE:relinker>)
        add_test(NAME optional_plt COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_optional_plt.py $<TARGET_FILE:relinker>)
        add_test(NAME empty_tls COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_empty_tls.py $<TARGET_FILE:relinker>)
        add_test(NAME tls_function_coverage COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_tls_function_coverage.py $<TARGET_FILE:relinker>)
        add_test(NAME tls_diagnostics COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_tls_diagnostics.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_load_alignment COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_load_alignment.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_eh_frame_header COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_eh_frame_header.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_dynamic_strings COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_dynamic_strings.py $<TARGET_FILE:relinker>)
        add_test(NAME executable_segments COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_executable_segments.py $<TARGET_FILE:relinker>)
        add_test(NAME linux_entry_argv COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_linux_entry_argv.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_gui COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_gui.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_address_space COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_address_space.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_icon COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_icon.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_intel_trampolines COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_intel_trampolines.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_tls COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_tls.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_needed_modules COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_needed_modules.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_module_directories COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_module_directories.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_symbol_names COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_symbol_names.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_registry COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_registry.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_host_libc COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_host_libc.py $<TARGET_FILE:relinker>)
        add_test(NAME guest_absolute_symbols COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_guest_absolute_symbols.py $<TARGET_FILE:relinker>)
        add_test(NAME windows_import_modules COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_windows_import_modules.py $<TARGET_FILE:relinker>)
        add_test(NAME elf_header COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_elf_header.py $<TARGET_FILE:relinker>)
        add_test(NAME input_magic COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_input_magic.py $<TARGET_FILE:relinker>)
        add_test(NAME string_table_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_string_table_bounds.py $<TARGET_FILE:relinker>)
        add_test(NAME call_registry COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_call_registry.py $<TARGET_FILE:relinker>)
        add_test(NAME jmprel_table_bounds COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_jmprel_table_bounds.py $<TARGET_FILE:relinker>)
    else()
        message(STATUS "Python3 interpreter not found; skipping Python relinker tests")
    endif()
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
        add_test_executable(guest_intel_trampolines_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/GuestIntelTrampolinesTests.cpp
        )
        add_test(NAME guest_intel_trampolines_execution COMMAND guest_intel_trampolines_tests $<TARGET_FILE:relinker>)
    endif()
endif()
