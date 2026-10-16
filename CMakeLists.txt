cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_APPLE AND NOT ANYPS5_RELINKER_ONLY)
    set(CMAKE_APPLE_SILICON_PROCESSOR x86_64 CACHE STRING "Host processor CMake reports on Apple silicon")
    set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "Architectures built on macOS")
endif()
project(Relinker CXX)

set(ANYPS5_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

function(add_test_executable target)
    add_executable(${target} ${ARGN})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}>")
endfunction()

option(BUILD_TESTING "Build and register relinker tests" OFF)
option(ANYPS5_RELINKER_ONLY "Build the relinker without system libraries or third-party dependencies" OFF)
option(ANYPS5_BUILD_WASM "Build the browser relinker" OFF)
if(ANYPS5_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "ANYPS5_BUILD_WASM requires emcmake and Emscripten")
    endif()
    if(BUILD_TESTING)
        message(FATAL_ERROR "Use the native build for C++ tests and the browser tests for WASM")
    endif()
    set(ANYPS5_RELINKER_CLI OFF CACHE BOOL "Build the native relinker command line" FORCE)
    add_subdirectory(core/relinker)
    return()
endif()

if(BUILD_TESTING)
    enable_testing()
    if(MINGW)
        get_filename_component(testCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(runtimeName IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(NOT EXISTS "${testCompilerDirectory}/${runtimeName}")
                message(FATAL_ERROR "Cannot locate MinGW runtime: ${runtimeName}")
            endif()
            file(COPY "${testCompilerDirectory}/${runtimeName}" DESTINATION "${ANYPS5_TEST_OUTPUT_DIRECTORY}")
        endforeach()
    endif()
endif()

option(ANYPS5_ENABLE_SPIRV_TOOLS "Enable SPIRV-Tools validation and optimization" OFF)
option(ANYPS5_ENABLE_ASAN "Build Linux targets with AddressSanitizer and use an ASan-compatible guest arena range" OFF)
option(APS5_ENABLE_TIMING_LOG "Write frame timings asynchronously to frame-timing.log" OFF)
option(APS5_ENABLE_TRACY "Build the Tracy profiler client and report performance timer scopes as Tracy zones" OFF)
option(ANYPS5_USE_SYSTEM_DEPS "Resolve SDL2, freetype, Vulkan-Headers, SPIRV-Headers, glslang and SPIRV-Tools from the system instead of the bundled submodules" OFF)
add_compile_definitions(APS5_ENABLE_TIMING_LOG=$<BOOL:${APS5_ENABLE_TIMING_LOG}>)
include(cmake/SystemDeps.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(ANYPS5_ENABLE_ASAN)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "ANYPS5_ENABLE_ASAN is supported only on Linux")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "ANYPS5_ENABLE_ASAN requires GCC or Clang")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    add_compile_definitions(ANYPS5_ENABLE_ASAN=1)
elseif(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

if(APPLE AND NOT ANYPS5_RELINKER_ONLY)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>)
    add_link_options(-fexperimental-library)
endif()

if(BUILD_TESTING)
    add_test_executable(shader_cache_file_read_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/tests/CacheFileReadTests.cpp)
    target_include_directories(shader_cache_file_read_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler)
    add_test(NAME shader_cache_file_read COMMAND shader_cache_file_read_tests)
    set_tests_properties(shader_cache_file_read PROPERTIES TIMEOUT 20)
    add_test_executable(request_serializer_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/tests/RequestSerializerTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/ControlFlow/src/RequestSerializer.cpp)
    target_include_directories(request_serializer_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include)
    add_test(NAME request_serializer COMMAND request_serializer_tests)
    set_tests_properties(request_serializer PROPERTIES TIMEOUT 20)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME shader_cache_native_paths COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/tests/test_shader_cache_native_paths.py
            ${CMAKE_CURRENT_SOURCE_DIR}/core/shader/recompiler/ShaderCacheVersion.cmake ${CMAKE_COMMAND})
        set_tests_properties(shader_cache_native_paths PROPERTIES TIMEOUT 20)
        add_test(NAME pr_overlap_encoding COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap_encoding.py)
        add_test(NAME pr_overlap_dependencies COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap_dependencies.py)
        add_test(NAME pr_overlap_tools COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap.py)
        add_test(NAME progress_history COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress_history.py)
        add_test(NAME progress_report COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress.py)
        add_test(NAME progress_encoding COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress_encoding.py)
        add_test(NAME progress_render_sliver COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress_render_sliver.py)
        add_test(NAME progress_wrappers COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress_wrappers.py)
        add_test(NAME progress_read_encoding COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_progress_read_encoding.py)
        add_test(NAME import_audit COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_import_audit.py)
        add_test(NAME unwrap_self COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_unwrap_self.py)
        foreach(shard RANGE 0 5)
            if(shard EQUAL 0)
                set(conventions_test_name conventions_function_bodies)
            else()
                set(conventions_test_name conventions_function_bodies_shard_${shard})
            endif()
            add_test(NAME ${conventions_test_name} COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_comments.py --shard ${shard}/6)
            set_tests_properties(${conventions_test_name} PROPERTIES TIMEOUT 30)
        endforeach()
        add_test(NAME conventions_wrapped_declarations COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_wrapped_declarations.py)
        add_test(NAME conventions_ci_gate COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_ci_gate.py)
        add_test(NAME conventions_doc_links COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_doc_links.py)
        add_test(NAME test_registration COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_test_registration.py)
        add_test(NAME build_eligibility COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_build_eligibility.py)
        add_test(NAME check_skips COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_skips.py)
        add_test(NAME select_tests COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_select_tests.py)
        add_test(NAME check_main_sync COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_main_sync.py)
        add_test(NAME configure_seed COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_configure_seed.py)
        add_test(NAME stubgen COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_stubgen.py)
        add_test(NAME debt_split COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_debt_split.py)
        add_test(NAME stubgen_stdout_newlines COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_stubgen_stdout_newlines.py)
        add_test(NAME hardware_oracle COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/hw-oracle/test_hw_oracle.py)
        add_test(NAME hardware_oracle_compare COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/hw-oracle/test_compare_results.py)
        set_tests_properties(hardware_oracle_compare PROPERTIES TIMEOUT 20)
        add_test(NAME pr_overlap_exports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap_exports.py)
        add_test(NAME pr_overlap_wrapped_exports COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap_wrapped_exports.py)
        add_test(NAME pr_overlap_own_branch COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pr_overlap_own_branch.py)
        add_test(NAME nid_database_cache COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_nid_names.py)
        add_test(NAME release_package COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_package_release.py)
        add_test(NAME web_library_assets COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_build_web.py)
        set_tests_properties(web_library_assets PROPERTIES TIMEOUT 20)
        add_test(NAME pad_input COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_pad_input.py)
        set_tests_properties(pad_input PROPERTIES TIMEOUT 20)
        add_test(NAME trace_digest COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_trace_digest.py)
        add_test(NAME conventions_bom_shebang COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_bom_shebang.py)
        set_tests_properties(conventions_bom_shebang PROPERTIES TIMEOUT 20)
        add_test(NAME conventions_body_bom COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_body_bom.py)
        add_test(NAME conventions_blob_reads COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_blob_reads.py)
        add_test(NAME conventions_agent_files COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_agent_files.py)
        add_test(NAME conventions_pr_template COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_conventions_pr_template.py)
        add_test(NAME check_compile COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_check_compile.py)
        add_test(NAME performance_compare COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_performance_compare.py)
        set_tests_properties(performance_compare PROPERTIES TIMEOUT 20)
    endif()
endif()

if(ANYPS5_RELINKER_ONLY)
    add_subdirectory(core/relinker)
    return()
endif()

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    message(FATAL_ERROR "The full build on macOS is x86-64 only; configure without CMAKE_OSX_ARCHITECTURES or set it to x86_64")
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_X11 ON CACHE BOOL "" FORCE)
set(SDL_WAYLAND ON CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
if(NOT ANYPS5_USE_SYSTEM_DEPS)
    set(ANYPS5_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "")
    add_subdirectory(3rdparty/SDL2)
    set(CMAKE_EXE_LINKER_FLAGS "${ANYPS5_EXE_LINKER_FLAGS}")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        get_directory_property(ANYPS5_SDL_X11_XRANDR DIRECTORY 3rdparty/SDL2 DEFINITION HAVE_X11_XRANDR)
        if(NOT ANYPS5_SDL_X11_XRANDR)
            message(FATAL_ERROR "SDL has no XRandR support, so X11 display refresh rates cannot be queried. "
                "Install libxrandr-dev (Debian, Ubuntu) or libXrandr-devel (Fedora), "
                "enable SDL_X11_XRANDR, then rerun configuration.")
        endif()
        get_directory_property(ANYPS5_SDL_PIPEWIRE DIRECTORY 3rdparty/SDL2 DEFINITION HAVE_PIPEWIRE)
        get_directory_property(ANYPS5_SDL_PULSEAUDIO DIRECTORY 3rdparty/SDL2 DEFINITION HAVE_PULSEAUDIO)
        get_directory_property(ANYPS5_SDL_ALSA DIRECTORY 3rdparty/SDL2 DEFINITION HAVE_ALSA)
        if(NOT ANYPS5_SDL_PIPEWIRE AND NOT ANYPS5_SDL_PULSEAUDIO AND NOT ANYPS5_SDL_ALSA)
            message(FATAL_ERROR "SDL found no PipeWire, PulseAudio or ALSA development files, so audio output would be silent. "
                "Install libpipewire-0.3-dev, libpulse-dev and libasound2-dev (Debian, Ubuntu) "
                "or pipewire-devel, pulseaudio-libs-devel and alsa-lib-devel (Fedora), then rerun configuration.")
        endif()
    endif()
endif()
set(ARCHITECTURE x86_64)
add_subdirectory(3rdparty/ffmpeg-core)
if(MINGW)
    target_link_libraries(ffmpeg INTERFACE bcrypt ole32 user32 ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(ffmpeg INTERFACE LINKER:--exclude-libs,ALL)
endif()

if(APS5_ENABLE_TRACY)
    include(cmake/Tracy.cmake)
endif()

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
if(NOT ANYPS5_USE_SYSTEM_DEPS)
    add_subdirectory(3rdparty/freetype EXCLUDE_FROM_ALL)
    set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(WIN32)
        get_target_property(freetypeSources freetype SOURCES)
        list(FILTER freetypeSources EXCLUDE REGEX "ftver\\.rc$")
        set_target_properties(freetype PROPERTIES SOURCES "${freetypeSources}")
    endif()
endif()

set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBUSB_ENABLE_UDEV OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/libusb-cmake EXCLUDE_FROM_ALL)
set_target_properties(usb-1.0 PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(cmake/ZArchive.cmake)

add_subdirectory(core/shader/recompiler)
add_subdirectory(core/Decoder/Jpeg)
add_subdirectory(core/Decoder/Png)
add_subdirectory(core/libs)
add_subdirectory(core/relinker)
