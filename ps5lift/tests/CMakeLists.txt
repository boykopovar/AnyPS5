find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(PS5LIFT_QEMU_TESTS "On an x86-64 host, also lift the fixtures for aarch64 and run them under qemu-aarch64" OFF)

set(PS5LIFT_FIXTURES basic vector float threads callbacks x87 setjmp cpu mmx modules strings sse41 avx)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PS5LIFT_X86_64_SYSROOT "" CACHE PATH "x86-64 Linux sysroot that the fixture guests build against")
    if(NOT IS_DIRECTORY "${PS5LIFT_X86_64_SYSROOT}/usr/x86_64-linux-gnu" OR NOT IS_DIRECTORY "${PS5LIFT_X86_64_SYSROOT}/usr/lib/gcc-cross/x86_64-linux-gnu")
        message(FATAL_ERROR "The fixture guests need an x86-64 Linux sysroot on ${CMAKE_SYSTEM_NAME}: set PS5LIFT_X86_64_SYSROOT to a directory "
                "with usr/x86_64-linux-gnu and usr/lib/gcc-cross/x86_64-linux-gnu, as Ubuntu's g++-x86-64-linux-gnu installs them, "
                "or configure with -DBUILD_TESTING=OFF.")
    endif()
    foreach(triple aarch64-unknown-linux-gnu x86_64-unknown-linux-gnu)
        string(REGEX REPLACE "-.*" "" arch ${triple})
        foreach(fixture IN LISTS PS5LIFT_FIXTURES)
            add_test(NAME lift_${fixture}_${arch}
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py fixture
                            --ps5lift $<TARGET_FILE:ps5lift>
                            --clang ${CMAKE_C_COMPILER}
                            --clangxx ${CMAKE_CXX_COMPILER}
                            --triple ${triple}
                            --fixture ${fixture}
                            --guest-sysroot ${PS5LIFT_X86_64_SYSROOT}
                            --lift-only)
        endforeach()
        add_test(NAME lift_basic_unoptimized_${arch}
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py fixture
                        --ps5lift $<TARGET_FILE:ps5lift>
                        --clang ${CMAKE_C_COMPILER}
                        --clangxx ${CMAKE_CXX_COMPILER}
                        --triple ${triple}
                        --fixture basic
                        --optimization=-O2
                        --unoptimized-lift
                        --guest-sysroot ${PS5LIFT_X86_64_SYSROOT}
                        --lift-only)
    endforeach()
    return()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(PS5LIFT_HOST_ARCH aarch64)
    set(PS5LIFT_TEST_TRIPLES aarch64-unknown-linux-gnu)
else()
    set(PS5LIFT_HOST_ARCH x86_64)
    set(PS5LIFT_TEST_TRIPLES x86_64-unknown-linux-gnu)
    if(PS5LIFT_QEMU_TESTS)
        list(APPEND PS5LIFT_TEST_TRIPLES aarch64-unknown-linux-gnu)
    endif()
endif()

foreach(triple IN LISTS PS5LIFT_TEST_TRIPLES)
    string(REGEX REPLACE "-.*" "" arch ${triple})
    foreach(fixture IN LISTS PS5LIFT_FIXTURES)
        set(memory_models)
        if(fixture STREQUAL "threads")
            set(memory_models --memory tso)
        endif()
        foreach(level O0 O1 O2 O3)
            add_test(NAME fixture_${fixture}_${level}_${arch}
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py fixture
                            --ps5lift $<TARGET_FILE:ps5lift>
                            --clang ${CMAKE_C_COMPILER}
                            --clangxx ${CMAKE_CXX_COMPILER}
                            --triple ${triple}
                            --fixture ${fixture}
                            --optimization=-${level}
                            ${memory_models})
        endforeach()
    endforeach()
    add_test(NAME fixture_basic_unoptimized_${arch}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py fixture
                    --ps5lift $<TARGET_FILE:ps5lift>
                    --clang ${CMAKE_C_COMPILER}
                    --clangxx ${CMAKE_CXX_COMPILER}
                    --triple ${triple}
                    --fixture basic
                    --optimization=-O2
                    --unoptimized-lift)
    add_test(NAME runtime_relink_${arch}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py runtime-relink
                    --triple ${triple}
                    --output ${CMAKE_BINARY_DIR}/runtime-relink-${triple}
                    --clang ${CMAKE_C_COMPILER}
                    --clangxx ${CMAKE_CXX_COMPILER})
    if(arch STREQUAL "aarch64")
        add_test(NAME runtime_without_sve_aarch64
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps5lift.py runtime-sve
                        --triple ${triple}
                        --cpu generic
                        --output ${CMAKE_BINARY_DIR}/runtime-${triple}-generic
                        --clang ${CMAKE_C_COMPILER}
                        --clangxx ${CMAKE_CXX_COMPILER})
    endif()
endforeach()
