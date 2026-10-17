set(ANYPS5_WASM_FLAGS --target=wasm64 -pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
add_compile_options(${ANYPS5_WASM_FLAGS} -DAPS5_ENABLE_TIMING_LOG=0)
add_link_options(${ANYPS5_WASM_FLAGS})

set(ANYPS5_LIBS_DIR ${PROJECT_SOURCE_DIR}/core/libs)
set(ANYPS5_WASM_AOT_DIR ${PROJECT_SOURCE_DIR}/core/wasm-aot)
set(ANYPS5_WASM_RUNTIME_PATCH ${CMAKE_BINARY_DIR}/wasm-aot/release/anyps5-wasm-runtime-patch)

file(GLOB_RECURSE ANYPS5_WASM_AOT_SOURCES CONFIGURE_DEPENDS ${ANYPS5_WASM_AOT_DIR}/src/*.rs)
add_custom_command(
        OUTPUT ${ANYPS5_WASM_RUNTIME_PATCH}
        COMMAND cargo build --release --quiet --manifest-path ${ANYPS5_WASM_AOT_DIR}/Cargo.toml --target-dir ${CMAKE_BINARY_DIR}/wasm-aot --bin anyps5-wasm-runtime-patch
        DEPENDS ${ANYPS5_WASM_AOT_SOURCES} ${ANYPS5_WASM_AOT_DIR}/Cargo.toml ${ANYPS5_WASM_AOT_DIR}/Cargo.lock
        COMMENT "Building the runtime patch tool"
        VERBATIM
)
add_custom_target(anyps5_wasm_runtime_patch DEPENDS ${ANYPS5_WASM_RUNTIME_PATCH})

add_library(aps5_wasm_runtime OBJECT
        src/Builtins.cpp
        src/DynamicLinker.cpp
        src/GuestCall.cpp
        src/Metadata.cpp
        src/PosixShims.cpp
        src/Program.cpp
        src/Thread.cpp
)
target_include_directories(aps5_wasm_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${ANYPS5_LIBS_DIR})

add_library(aps5_wasm_guest_memory OBJECT
        ${ANYPS5_LIBS_DIR}/prx/libc/src/GuestArena.cpp
        ${ANYPS5_LIBS_DIR}/prx/libc/src/GuestWriteWatch.cpp
)
target_include_directories(aps5_wasm_guest_memory PRIVATE ${ANYPS5_LIBS_DIR})

function(anyps5_wasm_runtime_executable target)
    add_executable(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp ${ARGN})
    target_link_libraries(${target} PRIVATE aps5_wasm_runtime)
    set_target_properties(${target} PROPERTIES SUFFIX .js)
    target_link_options(${target} PRIVATE
            -sPROXY_TO_PTHREAD
            -sEXIT_RUNTIME
            -sALLOW_MEMORY_GROWTH
            -sMAXIMUM_MEMORY=16GB
            -sSTACK_SIZE=2MB
            -sDEFAULT_PTHREAD_STACK_SIZE=2MB
            -sEXCEPTION_STACK_TRACES
            -Wno-pthreads-mem-growth
            -Wno-js-compiler
            -O2
            --pre-js=${CMAKE_CURRENT_SOURCE_DIR}/js/pre.js
            --post-js=${CMAKE_CURRENT_SOURCE_DIR}/js/post.js
    )
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/js/pre.js ${CMAKE_CURRENT_SOURCE_DIR}/js/post.js)
    add_dependencies(${target} anyps5_wasm_runtime_patch)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${ANYPS5_WASM_RUNTIME_PATCH} $<TARGET_FILE_DIR:${target}>/${target}.wasm -o $<TARGET_FILE_DIR:${target}>/${target}.wasm
            COMMENT "Routing host calls of guest function pointers in ${target}"
            VERBATIM
    )
endfunction()

anyps5_wasm_runtime_executable(anyps5-runtime-test
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestHost.cpp
        $<TARGET_OBJECTS:aps5_wasm_guest_memory>
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ANYPS5_WASM_THUNKS ${PROJECT_SOURCE_DIR}/tools/wasm_nid_thunks.py)
set(ANYPS5_WASM_LIBRARIES
        libc
        libkernel
        libSceCommonDialog
        libSceLibcInternal
        libSceNpGameIntent
        libSceNpTrophy2
        libSceNpUniversalDataSystem
        libScePad
        libSceSaveData.native
        libSceSaveDataDialog.native
        libSceSysmodule
        libSceSystemService
        libSceUlt
        libSceUserService
        libSceAgc
        libSceAgcDriver
        libSceAudioOut
        libSceVideoOut
)
set(ANYPS5_WASM_LAYOUT ${CMAKE_CURRENT_BINARY_DIR}/wasm-libraries.cmake)
execute_process(
        COMMAND ${Python3_EXECUTABLE} ${ANYPS5_WASM_THUNKS} layout --root ${PROJECT_SOURCE_DIR} --output ${ANYPS5_WASM_LAYOUT} ${ANYPS5_WASM_LIBRARIES}
        RESULT_VARIABLE ANYPS5_WASM_LAYOUT_RESULT
)
if(NOT ANYPS5_WASM_LAYOUT_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot read the library source lists")
endif()
include(${ANYPS5_WASM_LAYOUT})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ANYPS5_WASM_THUNKS})

function(anyps5_wasm_library library)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "EXCLUDE;SOURCES")
    string(MAKE_C_IDENTIFIER ${library} variable)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ANYPS5_WASM_${variable}_CMAKE})
    set(sources ${ANYPS5_WASM_${variable}_SOURCES})
    foreach(other IN LISTS ANYPS5_WASM_LIBRARIES)
        if(NOT other STREQUAL library)
            list(FILTER sources EXCLUDE REGEX "^${ANYPS5_LIBS_DIR}/prx/${other}/")
        endif()
    endforeach()
    foreach(excluded IN LISTS ARG_EXCLUDE)
        list(FILTER sources EXCLUDE REGEX "/${excluded}$")
    endforeach()
    list(APPEND sources ${ARG_SOURCES})
    set(includes ${ANYPS5_LIBS_DIR} ${ANYPS5_WASM_${variable}_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    list(REMOVE_DUPLICATES includes)
    set(definitions ${ANYPS5_WASM_${variable}_DEFINITIONS})
    list(TRANSFORM includes PREPEND -I OUTPUT_VARIABLE includeFlags)
    list(TRANSFORM definitions PREPEND -D OUTPUT_VARIABLE definitionFlags)
    set(wrappers)
    foreach(source IN LISTS sources)
        file(RELATIVE_PATH relative ${PROJECT_SOURCE_DIR} ${source})
        set(wrapper ${CMAKE_CURRENT_BINARY_DIR}/thunks/${library}/${relative})
        add_custom_command(
                OUTPUT ${wrapper}
                COMMAND ${Python3_EXECUTABLE} ${ANYPS5_WASM_THUNKS} wrap ${source} ${wrapper} -- ${CMAKE_CXX_COMPILER} --target=wasm64 -pthread -std=c++20 ${includeFlags} ${definitionFlags}
                DEPENDS ${source} ${ANYPS5_WASM_THUNKS}
                IMPLICIT_DEPENDS CXX ${source}
                COMMENT "Scanning ${relative} for guest exports"
                VERBATIM
        )
        list(APPEND wrappers ${wrapper})
    endforeach()
    add_library(aps5_wasm_${variable} OBJECT ${wrappers})
    target_include_directories(aps5_wasm_${variable} PRIVATE ${includes})
    target_compile_definitions(aps5_wasm_${variable} PRIVATE ${definitions})
endfunction()

set(ANYPS5_RECOMPILER_DIR ${PROJECT_SOURCE_DIR}/core/shader/recompiler)
file(GLOB_RECURSE ANYPS5_WASM_RECOMPILER_SOURCES CONFIGURE_DEPENDS ${ANYPS5_RECOMPILER_DIR}/*.cpp)
list(FILTER ANYPS5_WASM_RECOMPILER_SOURCES EXCLUDE REGEX "^${ANYPS5_RECOMPILER_DIR}/(tests/|WgslBackend/tools/|Deserialize\\.cpp$|SpirvBackend/src/SpirvOptimizer\\.cpp$)")
set(ANYPS5_WASM_RECOMPILER_VERSION ${CMAKE_CURRENT_BINARY_DIR}/ShaderCacheVersion.cpp)
file(CONFIGURE OUTPUT ${ANYPS5_WASM_RECOMPILER_VERSION} CONTENT "#include \"ShaderCacheSourceVersion.hpp\"\n\nnamespace ShaderRecompiler::ShaderDiskCache::Generated {\n\nconstinit const std::uint64_t SourceVersion = 0;\nconstinit const std::span<const std::string_view> RecompilerSwitches = {};\n\n}\n")
set(ANYPS5_WASM_RECOMPILER_INCLUDES
        ${ANYPS5_LIBS_DIR}
        ${ANYPS5_RECOMPILER_DIR}
        ${ANYPS5_RECOMPILER_DIR}/RdnaDecoder/include
        ${ANYPS5_RECOMPILER_DIR}/ControlFlow/include
        ${ANYPS5_RECOMPILER_DIR}/IntermediateRepresentation/include
        ${ANYPS5_RECOMPILER_DIR}/Translation/include
        ${ANYPS5_RECOMPILER_DIR}/Optimization/include
        ${ANYPS5_RECOMPILER_DIR}/SpirvBackend/include
        ${ANYPS5_RECOMPILER_DIR}/WgslBackend/include
        ${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)
add_library(aps5_wasm_recompiler OBJECT ${ANYPS5_WASM_RECOMPILER_SOURCES} ${ANYPS5_WASM_RECOMPILER_VERSION})
target_include_directories(aps5_wasm_recompiler PRIVATE ${ANYPS5_WASM_RECOMPILER_INCLUDES})
target_compile_definitions(aps5_wasm_recompiler PRIVATE ANYPS5_ENABLE_SPIRV_TOOLS=0)

set(ANYPS5_AGC_DRIVER_DIR ${ANYPS5_LIBS_DIR}/prx/libSceAgcDriver)
add_library(aps5_wasm_gpu_state OBJECT
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/ColorTargetLayout.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/DccMetadata.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/DepthSurface.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/GuestSamplerResource.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/GuestTextureResource.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/ShaderInputState.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/State.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/TextureFormat.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Graphics/src/TextureTiling.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Execution/src/ShaderMemory.cpp
)
target_include_directories(aps5_wasm_gpu_state PRIVATE ${ANYPS5_WASM_RECOMPILER_INCLUDES} ${PROJECT_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_compile_definitions(aps5_wasm_gpu_state PRIVATE ANYPS5_ENABLE_SPIRV_TOOLS=0)

include(${PROJECT_SOURCE_DIR}/cmake/ZArchive.cmake)
list(APPEND ANYPS5_WASM_libc_INCLUDES ${ZARCHIVE_SOURCE_DIR}/include)
anyps5_wasm_library(libc)
anyps5_wasm_library(libkernel)
set(ANYPS5_WASM_libSceAgcDriver_SOURCES
        ${ANYPS5_AGC_DRIVER_DIR}/Unimplemented.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Eq/src/Event.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Eq/src/Query.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Resource/src/Lifecycle.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Resource/src/Registration.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/State/src/Configuration.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/State/src/Status.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Submit/src/Acb.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Submit/src/CommandBuffer.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Submit/src/Dcb.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Execution/src/DisplayBuffer.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Execution/src/DisplayFormat.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Execution/src/HostHdr.cpp
        ${ANYPS5_AGC_DRIVER_DIR}/Execution/src/Pm4.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu/DrawStages.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu/Executor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu/GuestMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu/ShaderCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu/Shaders.cpp
)
set(ANYPS5_WASM_libSceAgcDriver_INCLUDES
        ${ANYPS5_WASM_RECOMPILER_INCLUDES}
        ${PROJECT_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
anyps5_wasm_library(libSceAgcDriver)
anyps5_wasm_library(libSceAgc)
anyps5_wasm_library(libSceAudioOut EXCLUDE AudioOutSdl.cpp AudioOut2Context.cpp AudioOut2Mastering.cpp AudioOut2PadMix.cpp AudioOut2Port.cpp AudioOut2SpeakerArray.cpp AudioOut2System.cpp AudioOut2User.cpp)
anyps5_wasm_library(libSceVideoOut EXCLUDE DisplayWindow.cpp HostHdrProbe.cpp ImeTextInput.cpp InputMapping.cpp KeyboardInput.cpp KeyboardMouse.cpp MouseInput.cpp PadInput.cpp)
target_include_directories(aps5_wasm_libSceVideoOut PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
foreach(library IN ITEMS libSceCommonDialog libSceLibcInternal libSceNpGameIntent libSceNpTrophy2 libSceNpUniversalDataSystem libScePad
        libSceSaveData.native libSceSaveDataDialog.native libSceSysmodule libSceSystemService libSceUlt libSceUserService)
    anyps5_wasm_library(${library})
endforeach()

set(ANYPS5_WASM_LIBRARY_OBJECTS)
foreach(library IN LISTS ANYPS5_WASM_LIBRARIES)
    string(MAKE_C_IDENTIFIER ${library} variable)
    list(APPEND ANYPS5_WASM_LIBRARY_OBJECTS $<TARGET_OBJECTS:aps5_wasm_${variable}>)
endforeach()
add_library(aps5_wasm_null_backend OBJECT gpu/NullBackend.cpp)
target_include_directories(aps5_wasm_null_backend PRIVATE ${ANYPS5_WASM_RECOMPILER_INCLUDES} ${PROJECT_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
add_library(aps5_wasm_null_platform OBJECT platform/NullPlatform.cpp)
target_include_directories(aps5_wasm_null_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

anyps5_wasm_runtime_executable(anyps5-runtime-node ${ANYPS5_WASM_LIBRARY_OBJECTS} $<TARGET_OBJECTS:aps5_wasm_recompiler> $<TARGET_OBJECTS:aps5_wasm_gpu_state> $<TARGET_OBJECTS:aps5_wasm_null_backend> $<TARGET_OBJECTS:aps5_wasm_null_platform>)
target_link_libraries(anyps5-runtime-node PRIVATE zarchive)
target_link_options(anyps5-runtime-node PRIVATE -sNODERAWFS -sENVIRONMENT=node)

add_library(aps5_wasm_webgpu_backend OBJECT gpu/WebGpuBackend.cpp)
target_include_directories(aps5_wasm_webgpu_backend PRIVATE ${ANYPS5_WASM_RECOMPILER_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_compile_options(aps5_wasm_webgpu_backend PRIVATE --use-port=emdawnwebgpu)

add_library(aps5_wasm_web_platform OBJECT platform/WebInput.cpp platform/WebAudio.cpp platform/WebStorage.cpp)
target_include_directories(aps5_wasm_web_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/platform ${ANYPS5_LIBS_DIR})

anyps5_wasm_runtime_executable(anyps5-runtime-web ${ANYPS5_WASM_LIBRARY_OBJECTS} $<TARGET_OBJECTS:aps5_wasm_recompiler> $<TARGET_OBJECTS:aps5_wasm_gpu_state> $<TARGET_OBJECTS:aps5_wasm_webgpu_backend> $<TARGET_OBJECTS:aps5_wasm_web_platform>)
target_link_libraries(anyps5-runtime-web PRIVATE zarchive)
target_link_options(anyps5-runtime-web PRIVATE
        --use-port=emdawnwebgpu
        -sENVIRONMENT=web,worker
        -sFORCE_FILESYSTEM
        -lidbfs.js
        -sOFFSCREENCANVAS_SUPPORT
        "-sOFFSCREENCANVASES_TO_PTHREAD=''"
        -sEXPORTED_RUNTIME_METHODS=ENV,FS,FS_createPath,FS_createDataFile,FS_createPreloadedFile,addRunDependency,removeRunDependency
)

set(ANYPS5_NODE node CACHE FILEPATH "Node.js 26 or newer for the WebAssembly tests")

add_executable(anyps5-platform-tests tests/PlatformTests.cpp)
set_target_properties(anyps5-platform-tests PROPERTIES SUFFIX .js)
target_include_directories(anyps5-platform-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/platform ${ANYPS5_LIBS_DIR})
target_link_options(anyps5-platform-tests PRIVATE -sENVIRONMENT=node -sEXIT_RUNTIME)
add_test(NAME wasm_platform COMMAND ${ANYPS5_NODE} $<TARGET_FILE:anyps5-platform-tests>)
add_test(NAME wasm_web_scripts COMMAND ${ANYPS5_NODE} --test ${CMAKE_CURRENT_SOURCE_DIR}/web/tests/*.test.mjs)
