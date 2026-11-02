cmake_minimum_required(VERSION 3.20)
project(AnyPS5LibraryPortability CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
get_filename_component(libs "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(guest_long_double_tests ../GuestLongDouble.cpp)
target_include_directories(guest_long_double_tests PRIVATE ${libs})
add_library(library_portability_sources OBJECT
        ${libs}/prx/libSceNet/Export.cpp
        ${libs}/prx/libSceSysmodule/Export.cpp
        ${libs}/prx/libc/src/ApplicationProcessParameters.cpp
        ${libs}/prx/libc/src/Formatting.cpp
        ${libs}/prx/libc/src/General.cpp
        ${libs}/prx/libc/src/GuestAllocations.cpp
        ${libs}/prx/libc/src/GuestArena.cpp
        ${libs}/prx/libc/src/Interfaces.cpp
        ${libs}/prx/libc/src/ScanningWide.cpp
        ${libs}/prx/libc/src/Strings.cpp
        ${libs}/prx/libc/src/specifics/linux/DlIteratePhdr.cpp
        ${libs}/prx/libkernel/DirectMemory/DirectMemory.cpp
        ${libs}/prx/libkernel/Equeue/src/FileEvent.cpp
        ${libs}/prx/libkernel/File/src/NativeStat.cpp
        ${libs}/prx/libkernel/File/src/Stdio.cpp
        ${libs}/prx/libkernel/Module/src/Module.cpp
        ${libs}/prx/libkernel/System/src/Process.cpp
        ${libs}/prx/libkernel/System/src/UserContext.cpp
)
target_include_directories(library_portability_sources PRIVATE ${libs})
if(EMSCRIPTEN)
    set(flags --target=wasm64 -pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
    target_compile_options(guest_long_double_tests PRIVATE ${flags})
    target_link_options(guest_long_double_tests PRIVATE ${flags} -sENVIRONMENT=node -sEXIT_RUNTIME)
    set_target_properties(guest_long_double_tests PROPERTIES SUFFIX .js)
    set(ANYPS5_NODE node CACHE FILEPATH "Node.js 26 or newer for memory64")
    add_test(NAME guest_long_double COMMAND ${ANYPS5_NODE} $<TARGET_FILE:guest_long_double_tests>)
    target_compile_options(library_portability_sources PRIVATE ${flags})
    add_executable(emscripten_module_unwind_tests
            ../EmscriptenModuleUnwind.cpp
            ${libs}/prx/libkernel/Module/src/Module.cpp
            ${libs}/prx/libSceSysmodule/Export.cpp)
    target_include_directories(emscripten_module_unwind_tests PRIVATE ${libs})
    target_compile_options(emscripten_module_unwind_tests PRIVATE ${flags})
    target_link_options(emscripten_module_unwind_tests PRIVATE ${flags} -sENVIRONMENT=node -sEXIT_RUNTIME)
    set_target_properties(emscripten_module_unwind_tests PROPERTIES SUFFIX .js)
    add_test(NAME emscripten_module_unwind COMMAND ${ANYPS5_NODE} $<TARGET_FILE:emscripten_module_unwind_tests>)
    set_tests_properties(emscripten_module_unwind PROPERTIES TIMEOUT 20)
else()
    add_test(NAME guest_long_double COMMAND guest_long_double_tests)
endif()
set_tests_properties(guest_long_double PROPERTIES TIMEOUT 20)
