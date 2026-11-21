cmake_minimum_required(VERSION 3.20)
project(anyps5_guest_cpu LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(anyps5_guest_cpu STATIC src/GuestCpu.cpp src/GuestRuntime.cpp)
target_include_directories(anyps5_guest_cpu PUBLIC include)
option(ANYPS5_GUEST_CPU_TESTS "Build guest CPU tests" ON)
if(ANYPS5_GUEST_CPU_TESTS)
    enable_testing()
    add_executable(anyps5_guest_runtime_tests tests/GuestRuntimeTests.cpp)
    target_link_libraries(anyps5_guest_runtime_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_runtime COMMAND anyps5_guest_runtime_tests)
    add_executable(anyps5_guest_cpu_tests tests/GuestCpuTests.cpp)
    target_link_libraries(anyps5_guest_cpu_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu COMMAND anyps5_guest_cpu_tests)
    add_executable(anyps5_guest_cpu_division_tests tests/GuestCpuDivisionTests.cpp)
    target_link_libraries(anyps5_guest_cpu_division_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_division COMMAND anyps5_guest_cpu_division_tests)
    add_executable(anyps5_guest_cpu_signed_division_tests tests/GuestCpuSignedDivisionTests.cpp)
    target_link_libraries(anyps5_guest_cpu_signed_division_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_signed_division COMMAND anyps5_guest_cpu_signed_division_tests)
    # Platform-independent program-level compatibility corpus.
    add_executable(anyps5_guest_cpu_program_corpus_tests tests/GuestCpuProgramCorpusTests.cpp)
    target_link_libraries(anyps5_guest_cpu_program_corpus_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_program_corpus COMMAND anyps5_guest_cpu_program_corpus_tests)
    add_executable(anyps5_guest_cpu_byte_alu_tests tests/GuestCpuByteAluTests.cpp)
    target_link_libraries(anyps5_guest_cpu_byte_alu_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_byte_alu COMMAND anyps5_guest_cpu_byte_alu_tests)
    add_executable(anyps5_guest_cpu_indirect_control_tests tests/GuestCpuIndirectControlTests.cpp)
    target_link_libraries(anyps5_guest_cpu_indirect_control_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_indirect_control COMMAND anyps5_guest_cpu_indirect_control_tests)
    add_executable(anyps5_guest_cpu_carry_rotate_tests tests/GuestCpuCarryRotateTests.cpp)
    target_link_libraries(anyps5_guest_cpu_carry_rotate_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_carry_rotate COMMAND anyps5_guest_cpu_carry_rotate_tests)
    add_executable(anyps5_guest_cpu_word_byte_alu_tests tests/GuestCpuWordByteAluTests.cpp)
    target_link_libraries(anyps5_guest_cpu_word_byte_alu_tests PRIVATE anyps5_guest_cpu)
    add_test(NAME guest_cpu_word_byte_alu COMMAND anyps5_guest_cpu_word_byte_alu_tests)
    # Differential comparisons execute native x86-64 instructions and therefore
    # run only on x86-64 GCC/Clang hosts; iOS ARM64 remains compile-only.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" AND
       CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        add_executable(anyps5_guest_cpu_differential_tests tests/GuestCpuDifferentialTests.cpp)
        target_link_libraries(anyps5_guest_cpu_differential_tests PRIVATE anyps5_guest_cpu)
        add_test(NAME guest_cpu_differential COMMAND anyps5_guest_cpu_differential_tests)
        add_executable(anyps5_guest_cpu_carry_rotate_differential_tests tests/GuestCpuCarryRotateDifferentialTests.cpp)
        target_link_libraries(anyps5_guest_cpu_carry_rotate_differential_tests PRIVATE anyps5_guest_cpu)
        add_test(NAME guest_cpu_carry_rotate_differential COMMAND anyps5_guest_cpu_carry_rotate_differential_tests)
        add_executable(anyps5_guest_cpu_carry_arithmetic_differential_tests tests/GuestCpuCarryArithmeticDifferentialTests.cpp)
        target_link_libraries(anyps5_guest_cpu_carry_arithmetic_differential_tests PRIVATE anyps5_guest_cpu)
        add_test(NAME guest_cpu_carry_arithmetic_differential COMMAND anyps5_guest_cpu_carry_arithmetic_differential_tests)
        add_executable(anyps5_guest_cpu_word_byte_differential_tests tests/GuestCpuWordByteDifferentialTests.cpp)
        target_link_libraries(anyps5_guest_cpu_word_byte_differential_tests PRIVATE anyps5_guest_cpu)
        add_test(NAME guest_cpu_word_byte_differential COMMAND anyps5_guest_cpu_word_byte_differential_tests)
    endif()
    # Linux-only ELF Stage 3: compiler-generated function bytes, no relocations.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        find_program(STAGE3_CLANG NAMES clang-17 REQUIRED)
        find_program(STAGE3_NM NAMES nm REQUIRED)
        find_program(STAGE3_OBJCOPY NAMES objcopy REQUIRED)
        find_program(STAGE3_READELF NAMES readelf REQUIRED)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STAGE3_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/stage3_fixture.o")
        set(STAGE3_HEADER "${CMAKE_CURRENT_BINARY_DIR}/stage3_generated.hpp")
        add_custom_command(
            OUTPUT "${STAGE3_HEADER}"
            COMMAND "${STAGE3_CLANG}" -O1 -fno-stack-protector -fno-pic
                -fno-asynchronous-unwind-tables -fno-unwind-tables
                -fno-builtin -fcf-protection=none -falign-functions=1
                -falign-loops=1 -c
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage3_fixture.c"
                -o "${STAGE3_OBJECT}"
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_stage3_fixture.py"
                "${STAGE3_OBJECT}" "${STAGE3_HEADER}"
                "${STAGE3_NM}" "${STAGE3_OBJCOPY}" "${STAGE3_READELF}"
            DEPENDS tests/stage3_fixture.c tests/generate_stage3_fixture.py
            VERBATIM
        )
        add_custom_target(stage3_generate DEPENDS "${STAGE3_HEADER}")
        add_library(stage3_native OBJECT tests/stage3_fixture.c)
        add_executable(anyps5_guest_cpu_compiled_programs_tests
            tests/GuestCpuCompiledProgramsTests.cpp
            "${STAGE3_HEADER}"
            $<TARGET_OBJECTS:stage3_native>)
        add_dependencies(anyps5_guest_cpu_compiled_programs_tests stage3_generate)
        target_include_directories(anyps5_guest_cpu_compiled_programs_tests
            PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(anyps5_guest_cpu_compiled_programs_tests
            PRIVATE anyps5_guest_cpu)
        add_test(NAME guest_cpu_compiled_programs
            COMMAND anyps5_guest_cpu_compiled_programs_tests)
    endif()
endif()
