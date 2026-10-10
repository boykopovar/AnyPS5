function(ensure_agc_glslang)
    if(TARGET glslang AND TARGET SPIRV AND TARGET glslang-standalone)
        return()
    endif()

    if(TARGET glslang)
        if(NOT TARGET SPIRV)
            message(FATAL_ERROR "glslang exists but SPIRV target is missing")
        endif()
        if(NOT TARGET glslang-standalone)
            message(FATAL_ERROR "glslang exists but glslang-standalone target is missing")
        endif()
        return()
    endif()

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_EXTERNAL OFF)
    set(GLSLANG_TESTS OFF)
    set(GLSLANG_ENABLE_INSTALL OFF)
    set(ENABLE_GLSLANG_BINARIES ON)
    set(ENABLE_SPIRV ON)
    set(ENABLE_SPVREMAPPER OFF)
    set(ENABLE_OPT OFF)

    add_subdirectory(
            "${CMAKE_SOURCE_DIR}/3rdparty/glslang"
            "${CMAKE_BINARY_DIR}/3rdparty/glslang"
            EXCLUDE_FROM_ALL
    )
endfunction()

ensure_agc_glslang()

set(agcTextureDetileShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/TextureDetile.comp)
set(agcTextureDetileGeneratedRoot ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(agcTextureDetileGeneratedDir ${agcTextureDetileGeneratedRoot}/prx/libSceAgcDriver/Graphics/shaders)
set(agcTextureDetileShaderSpv ${agcTextureDetileGeneratedDir}/TextureDetile.spv)
set(agcTextureDetileShaderHeader ${agcTextureDetileGeneratedDir}/TextureDetile_spv.h)
add_custom_command(
        OUTPUT "${agcTextureDetileShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcTextureDetileShaderSpv}" "${agcTextureDetileShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcTextureDetileShaderSpv} -DOUTPUT=${agcTextureDetileShaderHeader}
        -DSYMBOL=TEXTURE_DETILE_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcTextureDetileShaderSource}" glslang-standalone
        VERBATIM
)

set(agcMeshArgumentsShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MeshArguments.comp)
set(agcMeshArgumentsShaderSpv ${agcTextureDetileGeneratedDir}/MeshArguments.spv)
set(agcMeshArgumentsShaderHeader ${agcTextureDetileGeneratedDir}/MeshArguments_spv.h)
add_custom_command(
        OUTPUT "${agcMeshArgumentsShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcMeshArgumentsShaderSpv}" "${agcMeshArgumentsShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcMeshArgumentsShaderSpv} -DOUTPUT=${agcMeshArgumentsShaderHeader}
        -DSYMBOL=MESH_ARGUMENTS_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcMeshArgumentsShaderSource}" glslang-standalone
        VERBATIM
)

set(agcCmaskClearShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/CmaskClear.comp)
set(agcCmaskClearShaderSpv ${agcTextureDetileGeneratedDir}/CmaskClear.spv)
set(agcCmaskClearShaderHeader ${agcTextureDetileGeneratedDir}/CmaskClear_spv.h)
add_custom_command(
        OUTPUT "${agcCmaskClearShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcCmaskClearShaderSpv}" "${agcCmaskClearShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcCmaskClearShaderSpv} -DOUTPUT=${agcCmaskClearShaderHeader}
        -DSYMBOL=CMASK_CLEAR_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcCmaskClearShaderSource}" glslang-standalone
        VERBATIM
)

set(agcSampleCounterShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/SampleCounter.comp)
set(agcSampleCounterShaderSpv ${agcTextureDetileGeneratedDir}/SampleCounter.spv)
set(agcSampleCounterShaderHeader ${agcTextureDetileGeneratedDir}/SampleCounter_spv.h)
add_custom_command(
        OUTPUT "${agcSampleCounterShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcSampleCounterShaderSpv}" "${agcSampleCounterShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleCounterShaderSpv} -DOUTPUT=${agcSampleCounterShaderHeader}
        -DSYMBOL=SAMPLE_COUNTER_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleCounterShaderSource}" glslang-standalone
        VERBATIM
)

set(agcSampleDumpsShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/SampleDumps.comp)
set(agcSampleDumpsShaderSpv ${agcTextureDetileGeneratedDir}/SampleDumps.spv)
set(agcSampleDumpsShaderHeader ${agcTextureDetileGeneratedDir}/SampleDumps_spv.h)
add_custom_command(
        OUTPUT "${agcSampleDumpsShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcSampleDumpsShaderSpv}" "${agcSampleDumpsShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleDumpsShaderSpv} -DOUTPUT=${agcSampleDumpsShaderHeader}
        -DSYMBOL=SAMPLE_DUMPS_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleDumpsShaderSource}" glslang-standalone
        VERBATIM
)

set(agcDriverSources
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/DisplayBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/DisplayFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/DeviceAccess.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Diagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DataHitVerification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchCacheStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchInsert.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchLookup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchTiming.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/IndirectDispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/VariantValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCacheStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawDecode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawLookup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawPrecheck.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawRegisterKey.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawTiming.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/IndirectDraw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/EntryPoints.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Lifecycle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/BufferCopy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/BufferFill.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/CaptureDiagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/CaptureValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/WriteEvidence.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/WrittenBuffers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/CommandValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/PacketExecution.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/PacketFlush.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/PacketHistory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/PacketMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Packets/PacketTiming.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Presentation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Queues/QueueWorker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Queues/Submission.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Queues/WorkerAffinity.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Shaders/ShaderRegistry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/DeferredLabels.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/LabelHistory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/SynchronizationStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/WaitMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/WaitStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/WorkerSampler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ShaderMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/BdaFeatures.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/SubgroupClock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ShaderDeviceProfile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Pm4.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/VulkanDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/AspectFit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PresentationScaler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/DepthSurface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/MultisampleTarget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ColorTargetLayout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ColorTargetTransfer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PipelineCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Recorder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderInputState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderAddressResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestBufferMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/BdaResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/BufferAddress.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/State.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestTextureResource.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestSamplerResource.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/DccMetadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureTiling.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureDetiler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Texture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/UnitShadow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Sampler.cpp
        ${agcTextureDetileShaderHeader}
        ${agcCmaskClearShaderHeader}
        ${agcMeshArgumentsShaderHeader}
        ${agcSampleCounterShaderHeader}
        ${agcSampleDumpsShaderHeader}
        ${CMAKE_CURRENT_SOURCE_DIR}/Eq/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Eq/src/Query.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Resource/src/Lifecycle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Resource/src/Registration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/State/src/Configuration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/State/src/Status.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Submit/src/Acb.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Submit/src/CommandBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Submit/src/Dcb.cpp
)

add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL
        ${agcDriverSources}
)

target_include_directories(libSceAgcDriver PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)

file(GLOB_RECURSE agcShaderCacheGuardSources CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER agcShaderCacheGuardSources EXCLUDE REGEX "^(tests|tools)/")
add_shader_recompiler(shader_recompiler_agc_driver libc
        GUARD_SOURCES
        ${agcShaderCacheGuardSources}
)

target_link_libraries(libSceAgcDriver PRIVATE
        shader_recompiler_agc_driver
        SDL2-static
        libc
        libkernel
)

set_target_properties(libSceAgcDriver PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceAgcDriver)

if(BUILD_TESTING)
    add_test_executable(agc_write_watch_coverage_tests tests/WriteWatchCoverage.cpp)
    target_include_directories(agc_write_watch_coverage_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_write_watch_coverage_tests PRIVATE libc)
    configure_windows_unwind(agc_write_watch_coverage_tests)
    add_test(NAME agc_write_watch_coverage COMMAND agc_write_watch_coverage_tests)
    if(WIN32)
        set_tests_properties(agc_write_watch_coverage PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_profile_output_tests tests/ProfileOutput.cpp)
    target_include_directories(agc_profile_output_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_profile_output_tests PRIVATE libc)
    configure_windows_unwind(agc_profile_output_tests)
    add_test(NAME agc_profile_output COMMAND agc_profile_output_tests)
    set_tests_properties(agc_profile_output PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_profile_output PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(WIN32)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0602)
endif()

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()

add_test_executable(agc_driver_tests EXCLUDE_FROM_ALL
        tests/Submit.cpp
)

target_include_directories(agc_driver_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)

target_link_libraries(agc_driver_tests PRIVATE
        libSceAgcDriver
        libkernel
        libc
)

set_target_properties(agc_driver_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_tests)

add_test_executable(agc_shader_memory_tests EXCLUDE_FROM_ALL
        tests/ShaderMemory.cpp
        Execution/src/ShaderMemory.cpp
        Execution/src/GuestMemory.cpp
)
target_include_directories(agc_shader_memory_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_memory_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_memory_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_memory_tests)

add_test_executable(agc_shared_write_tests EXCLUDE_FROM_ALL tests/SharedWrites.cpp)
target_include_directories(agc_shared_write_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shared_write_tests PRIVATE libSceAgcDriver libkernel libc)
set_target_properties(agc_shared_write_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shared_write_tests)

add_test_executable(agc_write_tracking_tests EXCLUDE_FROM_ALL
        tests/WriteTracking.cpp
        Execution/src/GuestMemory.cpp
)
target_include_directories(agc_write_tracking_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_write_tracking_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_write_tracking_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_write_tracking_tests)
if(BUILD_TESTING)
    set_target_properties(agc_write_tracking_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_write_tracking COMMAND agc_write_tracking_tests)
    set_tests_properties(agc_write_tracking PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_write_tracking PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_structurizer_tests EXCLUDE_FROM_ALL tests/Structurizer.cpp)
target_link_libraries(agc_structurizer_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_structurizer_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_structurizer_tests)

add_test_executable(agc_fragment_barycentric_tests EXCLUDE_FROM_ALL tests/FragmentBarycentric.cpp)
target_include_directories(agc_fragment_barycentric_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
set_target_properties(agc_fragment_barycentric_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_fragment_barycentric_tests)
if(BUILD_TESTING)
    set_target_properties(agc_fragment_barycentric_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_fragment_barycentric COMMAND agc_fragment_barycentric_tests)
endif()

add_test_executable(agc_subgroup_clock_tests EXCLUDE_FROM_ALL
        tests/SubgroupClock.cpp
        Execution/src/SubgroupClock.cpp
)
target_include_directories(agc_subgroup_clock_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
set_target_properties(agc_subgroup_clock_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_subgroup_clock_tests)
if(BUILD_TESTING)
    set_target_properties(agc_subgroup_clock_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_subgroup_clock COMMAND agc_subgroup_clock_tests)
endif()

add_test_executable(agc_tessellation_factor_ring_tests EXCLUDE_FROM_ALL
        tests/TessellationFactorRing.cpp
        State/src/Configuration.cpp
)
target_include_directories(agc_tessellation_factor_ring_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_tessellation_factor_ring_tests PRIVATE libc)
set_target_properties(agc_tessellation_factor_ring_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_tessellation_factor_ring_tests)
if(BUILD_TESTING)
    set_target_properties(agc_tessellation_factor_ring_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_tessellation_factor_ring COMMAND agc_tessellation_factor_ring_tests)
    if(WIN32)
        set_tests_properties(agc_tessellation_factor_ring PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_spirv_module_tests EXCLUDE_FROM_ALL tests/SpirvModule.cpp)
target_link_libraries(agc_spirv_module_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_module_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_module_tests)

add_test_executable(agc_spirv_narrow_constant_store_tests EXCLUDE_FROM_ALL tests/SpirvNarrowConstantStore.cpp)
target_link_libraries(agc_spirv_narrow_constant_store_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_narrow_constant_store_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_narrow_constant_store_tests)
if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_spirv_narrow_constant_store_tests PRIVATE SPIRV-Tools-static)
    add_test_executable(agc_spirv_optimizer_tests EXCLUDE_FROM_ALL tests/SpirvOptimizer.cpp)
    target_link_libraries(agc_spirv_optimizer_tests PRIVATE shader_recompiler_agc_driver libc SPIRV-Tools-static)
    set_target_properties(agc_spirv_optimizer_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_spirv_optimizer_tests)
endif()

add_test_executable(agc_float16_conversion_tests EXCLUDE_FROM_ALL tests/Float16Conversion.cpp)
target_link_libraries(agc_float16_conversion_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_float16_conversion_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_float16_conversion_tests)
if(BUILD_TESTING)
    set_target_properties(agc_float16_conversion_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_float16_conversion COMMAND agc_float16_conversion_tests)
    set_tests_properties(agc_float16_conversion PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_float16_conversion PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_dpp_write_tests EXCLUDE_FROM_ALL tests/DppWrite.cpp)
target_link_libraries(agc_dpp_write_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_dpp_write_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_dpp_write_tests)

add_test_executable(agc_masked_select_tests EXCLUDE_FROM_ALL tests/MaskedSelect.cpp)
target_link_libraries(agc_masked_select_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_masked_select_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_masked_select_tests)

add_test_executable(agc_wave_control_decode_tests EXCLUDE_FROM_ALL tests/WaveControlDecode.cpp)
target_include_directories(agc_wave_control_decode_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_wave_control_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_wave_control_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_wave_control_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_wave_control_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_wave_control_decode COMMAND agc_wave_control_decode_tests)
    if(WIN32)
        set_tests_properties(agc_wave_control_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_denormal_flush_elimination_tests EXCLUDE_FROM_ALL tests/DenormalFlushElimination.cpp)
target_link_libraries(agc_denormal_flush_elimination_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_denormal_flush_elimination_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_denormal_flush_elimination_tests)
if(BUILD_TESTING)
    set_target_properties(agc_denormal_flush_elimination_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_denormal_flush_elimination COMMAND agc_denormal_flush_elimination_tests)
    if(WIN32)
        set_tests_properties(agc_denormal_flush_elimination PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_vop1_control_decode_tests EXCLUDE_FROM_ALL tests/Vop1ControlDecode.cpp)
target_link_libraries(agc_vop1_control_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop1_control_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop1_control_decode_tests)
add_test_executable(agc_vop2_encoding_decode_tests EXCLUDE_FROM_ALL tests/Vop2EncodingDecode.cpp)
target_link_libraries(agc_vop2_encoding_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop2_encoding_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop2_encoding_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_vop2_encoding_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vop2_encoding_decode COMMAND agc_vop2_encoding_decode_tests)
    if(WIN32)
        set_tests_properties(agc_vop2_encoding_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_dcc_key_sharing_tests EXCLUDE_FROM_ALL tests/DccKeySharing.cpp)
target_include_directories(agc_dcc_key_sharing_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_dcc_key_sharing_tests PRIVATE libc)
set_target_properties(agc_dcc_key_sharing_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_dcc_key_sharing_tests)
if(BUILD_TESTING)
    set_target_properties(agc_dcc_key_sharing_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_dcc_key_sharing COMMAND agc_dcc_key_sharing_tests)
    if(WIN32)
        set_tests_properties(agc_dcc_key_sharing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_buffer_pool_tests EXCLUDE_FROM_ALL tests/BufferPool.cpp)
target_include_directories(agc_buffer_pool_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_buffer_pool_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_buffer_pool_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_buffer_pool_tests)
if(BUILD_TESTING)
    set_target_properties(agc_buffer_pool_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_buffer_pool COMMAND agc_buffer_pool_tests)
    if(WIN32)
        set_tests_properties(agc_buffer_pool PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_display_buffer_dcc_tests EXCLUDE_FROM_ALL tests/DisplayBufferDcc.cpp)
target_include_directories(agc_display_buffer_dcc_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_display_buffer_dcc_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_display_buffer_dcc_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_display_buffer_dcc_tests)
if(BUILD_TESTING)
    set_target_properties(agc_display_buffer_dcc_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_display_buffer_dcc COMMAND agc_display_buffer_dcc_tests)
    if(WIN32)
        set_tests_properties(agc_display_buffer_dcc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_validate_program_tests EXCLUDE_FROM_ALL tests/ValidateProgram.cpp)
target_link_libraries(agc_validate_program_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_validate_program_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_validate_program_tests)

add_test_executable(agc_formatted_buffer_once_tests EXCLUDE_FROM_ALL tests/FormattedBufferOnce.cpp)
target_link_libraries(agc_formatted_buffer_once_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_formatted_buffer_once_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_formatted_buffer_once_tests)

add_test_executable(agc_shader_disk_cache_tests EXCLUDE_FROM_ALL tests/ShaderDiskCache.cpp)
target_include_directories(agc_shader_disk_cache_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_disk_cache_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_disk_cache_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_disk_cache_tests)

add_test_executable(agc_shader_device_profile_tests EXCLUDE_FROM_ALL tests/ShaderDeviceProfile.cpp Execution/src/ShaderDeviceProfile.cpp)
target_include_directories(agc_shader_device_profile_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_shader_device_profile_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_device_profile_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_device_profile_tests)

add_test_executable(agc_shader_control_flow_tests EXCLUDE_FROM_ALL
        tests/ControlFlow.cpp
)
target_include_directories(agc_shader_control_flow_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_control_flow_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_control_flow_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_control_flow_tests)

add_test_executable(agc_shader_mode_register_tests EXCLUDE_FROM_ALL tests/ShaderModeRegisters.cpp)
target_include_directories(agc_shader_mode_register_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_mode_register_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_mode_register_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_mode_register_tests)

add_test_executable(agc_driver_flip_tests EXCLUDE_FROM_ALL
        tests/Flip.cpp
)

target_include_directories(agc_driver_flip_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_flip_tests PRIVATE
        libSceAgcDriver
        libc
)

set_target_properties(agc_driver_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_flip_tests)

set(agcSampleLodShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/SampleLod.comp)
set(agcTestShaderGeneratedRoot ${CMAKE_CURRENT_BINARY_DIR}/generated-tests)
set(agcSampleLodShaderSpv ${agcTestShaderGeneratedRoot}/SampleLod.spv)
set(agcSampleLodShaderHeader ${agcTestShaderGeneratedRoot}/SampleLod_spv.h)
add_custom_command(
        OUTPUT "${agcSampleLodShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcSampleLodShaderSpv}" "${agcSampleLodShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleLodShaderSpv} -DOUTPUT=${agcSampleLodShaderHeader}
        -DSYMBOL=SAMPLE_LOD_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleLodShaderSource}" glslang-standalone
        VERBATIM
)

set(agcSampleArrayShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/SampleArray.comp)
set(agcSampleArrayShaderSpv ${agcTestShaderGeneratedRoot}/SampleArray.spv)
set(agcSampleArrayShaderHeader ${agcTestShaderGeneratedRoot}/SampleArray_spv.h)
add_custom_command(
        OUTPUT "${agcSampleArrayShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcSampleArrayShaderSpv}" "${agcSampleArrayShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleArrayShaderSpv} -DOUTPUT=${agcSampleArrayShaderHeader}
        -DSYMBOL=SAMPLE_ARRAY_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleArrayShaderSource}" glslang-standalone
        VERBATIM
)

foreach(agcTriangleStage IN ITEMS vert frag)
    set(agcTriangleSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/Triangle.${agcTriangleStage})
    set(agcTriangleSpv ${agcTestShaderGeneratedRoot}/Triangle_${agcTriangleStage}.spv)
    set(agcTriangleHeader ${agcTestShaderGeneratedRoot}/Triangle_${agcTriangleStage}_spv.h)
    add_custom_command(
            OUTPUT "${agcTriangleHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcTriangleSpv}" "${agcTriangleSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcTriangleSpv} -DOUTPUT=${agcTriangleHeader}
            -DSYMBOL=TRIANGLE_${agcTriangleStage}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcTriangleSource}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcTriangleHeaders "${agcTriangleHeader}")
endforeach()

foreach(agcCoverStage IN ITEMS vert frag)
    string(TOUPPER "${agcCoverStage}" agcCoverSymbol)
    set(agcCoverShaderSpv ${agcTestShaderGeneratedRoot}/Cover_${agcCoverStage}.spv)
    set(agcCoverShaderHeader ${agcTestShaderGeneratedRoot}/Cover_${agcCoverStage}_spv.h)
    add_custom_command(
            OUTPUT "${agcCoverShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcCoverShaderSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/Cover.${agcCoverStage}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcCoverShaderSpv} -DOUTPUT=${agcCoverShaderHeader}
            -DSYMBOL=COVER_${agcCoverSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/Cover.${agcCoverStage}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcCoverShaderHeaders ${agcCoverShaderHeader})
endforeach()

add_test_executable(agc_driver_cmask_fast_clear_tests EXCLUDE_FROM_ALL
        tests/CmaskFastClear.cpp
        ${agcCoverShaderHeaders}
)

target_include_directories(agc_driver_cmask_fast_clear_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)

target_link_libraries(agc_driver_cmask_fast_clear_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_cmask_fast_clear_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_cmask_fast_clear_tests)

add_test_executable(agc_driver_recorder_tests EXCLUDE_FROM_ALL
        tests/Recorder.cpp
        tests/ResidentPresent.cpp
        ${agcTriangleHeaders}
        ${agcSampleLodShaderHeader}
        ${agcSampleArrayShaderHeader}
        ${agcCoverShaderHeaders}
)

target_include_directories(agc_driver_recorder_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)

target_link_libraries(agc_driver_recorder_tests PRIVATE
        libkernel
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_recorder_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_recorder_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_recorder_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_pipeline_cache COMMAND agc_driver_recorder_tests --pipeline-cache-only)
    set_tests_properties(agc_pipeline_cache PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_cache PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_unchanged_cpu_stamps COMMAND agc_driver_recorder_tests --unchanged-cpu-stamps-only)
    set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_host_import_unmap COMMAND agc_driver_recorder_tests --host-import-unmap-only)
    set_tests_properties(agc_host_import_unmap PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_host_import_unmap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_recorder_kept_bytes_tests EXCLUDE_FROM_ALL tests/RecorderKeptBytes.cpp)
target_include_directories(agc_recorder_kept_bytes_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_recorder_kept_bytes_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_recorder_kept_bytes_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_recorder_kept_bytes_tests)
if(BUILD_TESTING)
    set_target_properties(agc_recorder_kept_bytes_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_recorder_kept_bytes COMMAND agc_recorder_kept_bytes_tests)
    if(WIN32)
        set_tests_properties(agc_recorder_kept_bytes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_driver_pm4_tests EXCLUDE_FROM_ALL
        tests/Pm4.cpp
)

target_include_directories(agc_driver_pm4_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_pm4_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)

set_target_properties(agc_driver_pm4_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_pm4_tests)

add_test_executable(agc_driver_graphics_tests EXCLUDE_FROM_ALL
        tests/Graphics.cpp
        tests/ColorTargetLayout.cpp
        tests/BdaResources.cpp
        tests/GuestAllocations.cpp
        tests/LiveStack.cpp
        tests/TextureFormat.cpp
        tests/TextureTiling.cpp
        tests/GuestTextureResource.cpp
        tests/GuestSamplerResource.cpp
        tests/TextureDetiler.cpp
        tests/DepthSurfaceReuse.cpp
        Graphics/src/Draw.cpp
        Graphics/src/DepthSurface.cpp
        Graphics/src/MultisampleTarget.cpp
        Graphics/src/Pipeline.cpp
        Graphics/src/State.cpp
        Graphics/src/ColorTargetLayout.cpp
        Graphics/src/ColorTargetTransfer.cpp
        Graphics/src/Resources.cpp
        Graphics/src/ShaderResources.cpp
        Graphics/src/ShaderAddressResources.cpp
        Graphics/src/GuestBufferMemory.cpp
        Graphics/src/BdaResources.cpp
        Graphics/src/BufferAddress.cpp
        Graphics/src/ShaderValidation.cpp
        Graphics/src/GuestTextureResource.cpp
        Graphics/src/GuestSamplerResource.cpp
        Graphics/src/TextureFormat.cpp
        Graphics/src/DccMetadata.cpp
        Graphics/src/TextureTiling.cpp
        Graphics/src/TextureDetiler.cpp
        Graphics/src/Texture.cpp
        Graphics/src/UnitShadow.cpp
        Graphics/src/Sampler.cpp
        Graphics/src/Recorder.cpp
        Graphics/src/ShaderInputState.cpp
        ${agcTextureDetileShaderHeader}
        ${agcCmaskClearShaderHeader}
        ${agcMeshArgumentsShaderHeader}
        ${agcSampleCounterShaderHeader}
        ${agcSampleDumpsShaderHeader}
        Execution/src/GuestMemory.cpp
        Execution/src/Pm4.cpp
)

target_include_directories(agc_driver_graphics_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)

target_link_libraries(agc_driver_graphics_tests PRIVATE
        shader_recompiler_agc_driver
        libc
        libkernel
)

set_target_properties(agc_driver_graphics_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_graphics_tests)

add_test_executable(agc_driver_bda_device_tests EXCLUDE_FROM_ALL
        tests/BdaDevice.cpp
        tests/BdaShader.cpp
        tests/BdaExecution.cpp
        tests/BdaContracts.cpp
        Execution/src/BdaFeatures.cpp
        Graphics/src/Resources.cpp
        Graphics/src/BufferAddress.cpp
)

target_include_directories(agc_driver_bda_device_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(agc_driver_bda_device_tests PRIVATE SDL2-static libc shader_recompiler_agc_driver)
set_target_properties(agc_driver_bda_device_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_bda_device_tests)

add_test_executable(agc_driver_mesh_tests EXCLUDE_FROM_ALL
        tests/Mesh.cpp
        ${agcDriverSources}
)
target_include_directories(agc_driver_mesh_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)
target_link_libraries(agc_driver_mesh_tests PRIVATE shader_recompiler_agc_driver SDL2-static libc libkernel)
set_target_properties(agc_driver_mesh_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_mesh_tests)

file(GLOB agcExecutionTestSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/*.cpp)
set(agcExecutionTests)
foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)
    get_filename_component(agcExecutionTestName ${agcExecutionTestSource} NAME_WE)
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" agcExecutionTestName "${agcExecutionTestName}")
    string(TOLOWER "agc_driver_${agcExecutionTestName}_tests" agcExecutionTest)
    add_test_executable(${agcExecutionTest} EXCLUDE_FROM_ALL ${agcExecutionTestSource})
    target_include_directories(${agcExecutionTest} PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
    )
    target_link_libraries(${agcExecutionTest} PRIVATE libSceAgcDriver SDL2-static libc)
    set_target_properties(${agcExecutionTest} PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(${agcExecutionTest})
    list(APPEND agcExecutionTests ${agcExecutionTest})
endforeach()

target_include_directories(agc_driver_packed_unorm_color_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

foreach(stage IN ITEMS vert frag)
    set(agcProvokingShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/ProvokingVertex.${stage})
    set(agcProvokingShaderSpv ${agcTestShaderGeneratedRoot}/ProvokingVertex_${stage}.spv)
    set(agcProvokingShaderHeader ${agcTestShaderGeneratedRoot}/ProvokingVertex_${stage}_spv.h)
    string(TOUPPER "PROVOKING_VERTEX_${stage}_SPV" agcProvokingShaderSymbol)
    add_custom_command(
        OUTPUT "${agcProvokingShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcProvokingShaderSpv}" "${agcProvokingShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcProvokingShaderSpv} -DOUTPUT=${agcProvokingShaderHeader}
        -DSYMBOL=${agcProvokingShaderSymbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcProvokingShaderSource}" glslang-standalone
        VERBATIM
    )
    target_sources(agc_driver_provoking_vertex_tests PRIVATE ${agcProvokingShaderHeader})
endforeach()
target_include_directories(agc_driver_provoking_vertex_tests PRIVATE ${agcTestShaderGeneratedRoot})

target_include_directories(agc_driver_prepared_exports_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_publication_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_graphics_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(agc_driver_interpolation_modes_tests PRIVATE shader_recompiler_agc_driver)

target_include_directories(agc_driver_prepared_shaders_tests PRIVATE
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
)

if(BUILD_TESTING)
    set_target_properties(agc_shader_mode_register_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_shader_mode_register COMMAND agc_shader_mode_register_tests)
    set_tests_properties(agc_shader_mode_register PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_shader_mode_register PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_bindless COMMAND agc_shader_memory_tests --bindless)
    set_tests_properties(agc_shader_bindless PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_frontend_pair COMMAND agc_shader_memory_tests --frontend-pair)
    add_test(NAME agc_shader_frontend_pair_off COMMAND agc_shader_memory_tests --frontend-pair)
    set_tests_properties(agc_shader_frontend_pair PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_shader_frontend_pair_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_FRONTEND_PAIR=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_shader_bindless agc_shader_frontend_pair agc_shader_frontend_pair_off PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(agcTest IN ITEMS agc_shared_write_tests agc_driver_tests agc_shader_memory_tests agc_structurizer_tests agc_spirv_module_tests agc_dpp_write_tests agc_masked_select_tests agc_vop1_control_decode_tests agc_validate_program_tests agc_shader_disk_cache_tests agc_shader_device_profile_tests agc_shader_control_flow_tests agc_driver_flip_tests agc_driver_pm4_tests agc_driver_graphics_tests agc_driver_bda_device_tests agc_driver_mesh_tests agc_driver_cmask_fast_clear_tests agc_formatted_buffer_once_tests ${agcExecutionTests})
        if(agcTest STREQUAL "agc_driver_constant_store_tests")
            continue()
        endif()
        string(REGEX REPLACE "_tests$" "" agcTestName "${agcTest}")
        set_target_properties(${agcTest} PROPERTIES EXCLUDE_FROM_ALL OFF)
        add_test(NAME ${agcTestName} COMMAND ${agcTest})
        set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_provoking_vertex PROPERTIES TIMEOUT 30)
    set_tests_properties(agc_driver_flat_store PROPERTIES RUN_SERIAL TRUE)
    set_tests_properties(agc_driver_storage_cache_budget PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sampled_cache_budget_memory COMMAND agc_driver_sampled_cache_budget_tests memory)
    set_tests_properties(agc_driver_sampled_cache_budget_memory PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_sampled_cache_budget_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_pm4_failure COMMAND agc_driver_pm4_tests failure)
    set_tests_properties(agc_driver_pm4_failure PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pm4_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_flip_compute COMMAND agc_driver_flip_tests compute)
    set_tests_properties(agc_driver_flip_compute PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_flip_compute PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    foreach(agcTest IN ITEMS agc_spirv_narrow_constant_store_tests agc_spirv_optimizer_tests agc_driver_constant_store_tests)
        if(NOT TARGET ${agcTest})
            continue()
        endif()
        set_target_properties(${agcTest} PROPERTIES EXCLUDE_FROM_ALL OFF)
        string(REGEX REPLACE "_tests$" "" agcTestBaseName "${agcTest}")
        foreach(agcSpirvMode IN ITEMS default full none)
            if(agcSpirvMode STREQUAL "default")
                set(agcTestName "${agcTestBaseName}")
                set(agcSpirvEnvironment "--unset=APS5_SPIRV_OPT")
            else()
                set(agcTestName "${agcTestBaseName}_${agcSpirvMode}")
                set(agcSpirvEnvironment "APS5_SPIRV_OPT=${agcSpirvMode}")
            endif()
            add_test(NAME ${agcTestName} COMMAND ${CMAKE_COMMAND} -E env
                    ${agcSpirvEnvironment} "$<TARGET_FILE:${agcTest}>")
            set_tests_properties(${agcTestName} PROPERTIES
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 60 SKIP_RETURN_CODE 77)
            if(agcTest STREQUAL "agc_driver_constant_store_tests")
                set_tests_properties(${agcTestName} PROPERTIES RUN_SERIAL TRUE)
            endif()
            if(WIN32)
                set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    foreach(preparedMode IN ITEMS indirect uncached debug debug_failure deferred deferred_undecodable)
        set(preparedTest "agc_driver_prepared_shaders_${preparedMode}")
        if(preparedMode STREQUAL "indirect")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --indirect)
        elseif(preparedMode STREQUAL "deferred")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --deferred)
        elseif(preparedMode STREQUAL "deferred_undecodable")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --deferred-undecodable)
        elseif(preparedMode STREQUAL "debug_failure")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --fail-before-registration)
            set_tests_properties(${preparedTest} PROPERTIES WILL_FAIL TRUE TIMEOUT 30)
        else()
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests)
        endif()
        set_tests_properties(${preparedTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(preparedMode STREQUAL "uncached")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_NO_DISPATCH_CACHE=1")
        elseif(preparedMode STREQUAL "debug" OR preparedMode STREQUAL "debug_failure")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_PROFILE_DRAW=1" "APS5_NO_UNLOCKED_DEVICE=1")
        endif()
        if(WIN32)
            set_tests_properties(${preparedTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_prepared_shaders
        agc_driver_prepared_shaders_indirect agc_driver_prepared_shaders_uncached
        agc_driver_prepared_shaders_debug agc_driver_prepared_shaders_debug_failure
        PROPERTIES RESOURCE_LOCK shader_failure_capture)
endif()

option(AGC_BUILD_VISUAL_TEST "Enable the standalone AGC SPIR-V visual test" OFF)

if(AGC_BUILD_VISUAL_TEST)
    ensure_agc_glslang()

    set(visualShaders)

    foreach(stage IN ITEMS vert frag)
        set(shaderOutput "${ANYPS5_TEST_OUTPUT_DIRECTORY}/Triangle.${stage}.spv")

        add_custom_command(
                OUTPUT "${shaderOutput}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${ANYPS5_TEST_OUTPUT_DIRECTORY}"
                COMMAND "$<TARGET_FILE:glslang-standalone>"
                -V
                --target-env vulkan1.1
                -o "${shaderOutput}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/Triangle.${stage}"
                DEPENDS
                glslang-standalone
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/Triangle.${stage}"
                VERBATIM
        )

        list(APPEND visualShaders "${shaderOutput}")
    endforeach()

    add_test_executable(agc_driver_visual_test EXCLUDE_FROM_ALL
            tests/Visual.cpp
            ${agcDriverSources}
            ${visualShaders}
    )

    target_include_directories(agc_driver_visual_test PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${agcTextureDetileGeneratedRoot}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
    )

    target_link_libraries(agc_driver_visual_test PRIVATE
            shader_recompiler_agc_driver
            SDL2-static
            libc
            libkernel
    )

    set_target_properties(agc_driver_visual_test PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_visual_test)

    if(WIN32)
        set(visualRuntimeFiles "$<TARGET_FILE:libc>" "$<TARGET_FILE:libkernel>")

        if(MINGW)
            get_filename_component(visualCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)

            foreach(runtimeName IN ITEMS
                    libgcc_s_seh-1.dll
                    libstdc++-6.dll
                    libwinpthread-1.dll
            )
                set(runtimePath "${visualCompilerDirectory}/${runtimeName}")

                if(NOT EXISTS "${runtimePath}")
                    message(FATAL_ERROR "Cannot locate MinGW runtime: ${runtimeName}")
                endif()

                list(APPEND visualRuntimeFiles "${runtimePath}")
            endforeach()
        endif()

        add_custom_target(agc_driver_visual_runtime
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${visualRuntimeFiles}
                "$<TARGET_FILE_DIR:agc_driver_visual_test>"
                DEPENDS libc libkernel
                COMMAND_EXPAND_LISTS
                VERBATIM
        )

        add_dependencies(agc_driver_visual_test agc_driver_visual_runtime)
    endif()
endif()

include(PostCompile.cmake)
# Replays shader_*.req files the driver writes when a compute dispatch cannot be recompiled.
add_executable(agc_shader_replay EXCLUDE_FROM_ALL
        tools/ShaderReplay.cpp
)
target_include_directories(agc_shader_replay PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_replay PRIVATE shader_recompiler_agc_driver libc)
if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_shader_replay PRIVATE SPIRV-Tools-static)
endif()
set_target_properties(agc_shader_replay PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_replay)

if(BUILD_TESTING)
    add_test_executable(texture_format_lookup_tests ${LIBS_INCLUDE_DIR}/tests/TextureFormatLookup.cpp)
    target_include_directories(texture_format_lookup_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
    target_link_libraries(texture_format_lookup_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(texture_format_lookup_tests)
    add_test(NAME texture_format_lookup COMMAND texture_format_lookup_tests)
    if(WIN32)
        set_tests_properties(texture_format_lookup PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(linear_display_buffer_tests ${LIBS_INCLUDE_DIR}/tests/LinearDisplayBuffer.cpp)
    target_include_directories(linear_display_buffer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(linear_display_buffer_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(linear_display_buffer_tests)
    add_test(NAME linear_display_buffer COMMAND linear_display_buffer_tests)
    if(WIN32)
        set_tests_properties(linear_display_buffer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_agc_driver_resources_tests ${LIBS_INCLUDE_DIR}/tests/GuestAgcDriverResources.cpp)
    target_include_directories(guest_agc_driver_resources_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_agc_driver_resources_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(guest_agc_driver_resources_tests)
    add_test(NAME guest_agc_driver_resources COMMAND guest_agc_driver_resources_tests)
    if(WIN32)
        set_tests_properties(guest_agc_driver_resources PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_rendering_wait_tests ${LIBS_INCLUDE_DIR}/tests/GuestRenderingWait.cpp)
    target_include_directories(guest_rendering_wait_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_rendering_wait_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(guest_rendering_wait_tests)
    add_test(NAME guest_rendering_wait COMMAND guest_rendering_wait_tests)
    if(WIN32)
        set_tests_properties(guest_rendering_wait PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(shader_buffer_address_tests ${LIBS_INCLUDE_DIR}/tests/ShaderBufferAddress.cpp)
    target_link_libraries(shader_buffer_address_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(shader_buffer_address_tests)
    add_test(NAME shader_buffer_address COMMAND shader_buffer_address_tests)
    if(WIN32)
        set_tests_properties(shader_buffer_address PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(ray_tracing_miss_tests ${LIBS_INCLUDE_DIR}/tests/RayTracingMiss.cpp)
    target_link_libraries(ray_tracing_miss_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(ray_tracing_miss_tests)
    add_test(NAME ray_tracing_miss COMMAND ray_tracing_miss_tests)
    if(WIN32)
        set_tests_properties(ray_tracing_miss PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(wave32_wide_subgroup_tests ${LIBS_INCLUDE_DIR}/tests/Wave32WideSubgroup.cpp)
    target_link_libraries(wave32_wide_subgroup_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(wave32_wide_subgroup_tests)
    add_test(NAME wave32_wide_subgroup COMMAND wave32_wide_subgroup_tests)
    if(WIN32)
        set_tests_properties(wave32_wide_subgroup PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_compare_i16_u16_tests ${LIBS_INCLUDE_DIR}/tests/VectorCompareI16U16.cpp)
    target_link_libraries(vector_compare_i16_u16_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_compare_i16_u16_tests)
    add_test(NAME vector_compare_i16_u16 COMMAND vector_compare_i16_u16_tests)
    if(WIN32)
        set_tests_properties(vector_compare_i16_u16 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_compare_const_tests ${LIBS_INCLUDE_DIR}/tests/VectorCompareConst.cpp)
    target_link_libraries(vector_compare_const_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_compare_const_tests)
    add_test(NAME vector_compare_const COMMAND vector_compare_const_tests)
    if(WIN32)
        set_tests_properties(vector_compare_const PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_min_max_med16_tests ${LIBS_INCLUDE_DIR}/tests/VectorMinMaxMed16.cpp)
    target_link_libraries(vector_min_max_med16_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_min_max_med16_tests)
    add_test(NAME vector_min_max_med16 COMMAND vector_min_max_med16_tests)
    if(WIN32)
        set_tests_properties(vector_min_max_med16 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_mad_legacy_tests ${LIBS_INCLUDE_DIR}/tests/VectorMadLegacy.cpp)
    target_link_libraries(vector_mad_legacy_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_mad_legacy_tests)
    add_test(NAME vector_mad_legacy COMMAND vector_mad_legacy_tests)
    if(WIN32)
        set_tests_properties(vector_mad_legacy PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_fused_multiply_add_tests ${LIBS_INCLUDE_DIR}/tests/VectorFusedMultiplyAdd.cpp)
    target_link_libraries(vector_fused_multiply_add_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_fused_multiply_add_tests)
    add_test(NAME vector_fused_multiply_add COMMAND vector_fused_multiply_add_tests)
    if(WIN32)
        set_tests_properties(vector_fused_multiply_add PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(vector_constant_factor_tininess_tests ${LIBS_INCLUDE_DIR}/tests/VectorConstantFactorTininess.cpp)
    target_link_libraries(vector_constant_factor_tininess_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(vector_constant_factor_tininess_tests)
    add_test(NAME vector_constant_factor_tininess COMMAND vector_constant_factor_tininess_tests)
    if(WIN32)
        set_tests_properties(vector_constant_factor_tininess PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(scalar_saveexec_tests ${LIBS_INCLUDE_DIR}/tests/ScalarSaveexec.cpp)
    target_link_libraries(scalar_saveexec_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(scalar_saveexec_tests)
    add_test(NAME scalar_saveexec COMMAND scalar_saveexec_tests)
    if(WIN32)
        set_tests_properties(scalar_saveexec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(scalar_swappc_tests ${LIBS_INCLUDE_DIR}/tests/ScalarSwappc.cpp)
    target_link_libraries(scalar_swappc_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(scalar_swappc_tests)
    add_test(NAME scalar_swappc COMMAND scalar_swappc_tests)
    if(WIN32)
        set_tests_properties(scalar_swappc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(scalar_bitcount64_tests ${LIBS_INCLUDE_DIR}/tests/ScalarBitCount64.cpp)
    target_link_libraries(scalar_bitcount64_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(scalar_bitcount64_tests)
    add_test(NAME scalar_bitcount64 COMMAND scalar_bitcount64_tests)
    if(WIN32)
        set_tests_properties(scalar_bitcount64 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(bitfield_mask_tests ${LIBS_INCLUDE_DIR}/tests/BitfieldMask.cpp)
    target_link_libraries(bitfield_mask_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(bitfield_mask_tests)
    add_test(NAME bitfield_mask COMMAND bitfield_mask_tests)
    if(WIN32)
        set_tests_properties(bitfield_mask PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(scalar_scratch_load_tests ${LIBS_INCLUDE_DIR}/tests/ScalarScratchLoad.cpp)
    target_link_libraries(scalar_scratch_load_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(scalar_scratch_load_tests)
    add_test(NAME scalar_scratch_load COMMAND scalar_scratch_load_tests)
    if(WIN32)
        set_tests_properties(scalar_scratch_load PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(scalar_call_tests ${LIBS_INCLUDE_DIR}/tests/ScalarCall.cpp)
    target_link_libraries(scalar_call_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(scalar_call_tests)
    add_test(NAME scalar_call COMMAND scalar_call_tests)
    if(WIN32)
        set_tests_properties(scalar_call PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(image_sample_gather_tests ${LIBS_INCLUDE_DIR}/tests/ImageSampleGather.cpp)
    target_link_libraries(image_sample_gather_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(image_sample_gather_tests)
    add_test(NAME image_sample_gather COMMAND image_sample_gather_tests)
    if(WIN32)
        set_tests_properties(image_sample_gather PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(pixel_ancillary_tests ${LIBS_INCLUDE_DIR}/tests/PixelAncillary.cpp)
    target_link_libraries(pixel_ancillary_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(pixel_ancillary_tests)
    add_test(NAME pixel_ancillary COMMAND pixel_ancillary_tests)
    if(WIN32)
        set_tests_properties(pixel_ancillary PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(guest_indexed_draw_tests ${LIBS_INCLUDE_DIR}/tests/GuestIndexedDraw.cpp)
    target_include_directories(guest_indexed_draw_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_indexed_draw_tests PRIVATE libSceAgc libSceAgcDriver libc)
    configure_windows_unwind(guest_indexed_draw_tests)
    add_test(NAME guest_indexed_draw COMMAND guest_indexed_draw_tests)
    if(WIN32)
        set_tests_properties(guest_indexed_draw PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_embedded_vertex_fetch_tests EXCLUDE_FROM_ALL tests/EmbeddedVertexFetch.cpp)
target_link_libraries(agc_embedded_vertex_fetch_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_embedded_vertex_fetch_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_embedded_vertex_fetch_tests)
if(BUILD_TESTING)
    set_target_properties(agc_embedded_vertex_fetch_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_embedded_vertex_fetch COMMAND agc_embedded_vertex_fetch_tests)
    if(WIN32)
        set_tests_properties(agc_embedded_vertex_fetch PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_rdna_truncated_modifier_tests EXCLUDE_FROM_ALL tests/RdnaTruncatedModifier.cpp)
target_link_libraries(agc_rdna_truncated_modifier_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_rdna_truncated_modifier_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_rdna_truncated_modifier_tests)
if(BUILD_TESTING)
    set_target_properties(agc_rdna_truncated_modifier_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_rdna_truncated_modifier COMMAND agc_rdna_truncated_modifier_tests)
    if(WIN32)
        set_tests_properties(agc_rdna_truncated_modifier PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_mubuf_lds_decode_tests EXCLUDE_FROM_ALL tests/MubufLdsDecode.cpp)
target_link_libraries(agc_mubuf_lds_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_mubuf_lds_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_mubuf_lds_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_mubuf_lds_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_mubuf_lds_decode COMMAND agc_mubuf_lds_decode_tests)
    if(WIN32)
        set_tests_properties(agc_mubuf_lds_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_vop1_sdwa_move_decode_tests EXCLUDE_FROM_ALL tests/Vop1SdwaMoveDecode.cpp)
target_link_libraries(agc_vop1_sdwa_move_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop1_sdwa_move_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop1_sdwa_move_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_vop1_sdwa_move_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vop1_sdwa_move_decode COMMAND agc_vop1_sdwa_move_decode_tests)
    if(WIN32)
        set_tests_properties(agc_vop1_sdwa_move_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_pixel_position_export_tests EXCLUDE_FROM_ALL tests/PixelPositionExport.cpp)
target_link_libraries(agc_pixel_position_export_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_pixel_position_export_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pixel_position_export_tests)
if(BUILD_TESTING)
    set_target_properties(agc_pixel_position_export_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_pixel_position_export COMMAND agc_pixel_position_export_tests)
    set_tests_properties(agc_pixel_position_export PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_pixel_position_export PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_position_invariance_tests EXCLUDE_FROM_ALL tests/PositionInvariance.cpp)
target_link_libraries(agc_position_invariance_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_position_invariance_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_position_invariance_tests)
if(BUILD_TESTING)
    set_target_properties(agc_position_invariance_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_position_invariance COMMAND agc_position_invariance_tests)
    set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(image_by_tests ${LIBS_INCLUDE_DIR}/tests/ImageBy.cpp)
    target_link_libraries(image_by_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(image_by_tests)
    add_test(NAME image_by COMMAND image_by_tests)
    if(WIN32)
        set_tests_properties(image_by PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(image_pck_n_tests ${LIBS_INCLUDE_DIR}/tests/ImagePckN.cpp)
    target_link_libraries(image_pck_n_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(image_pck_n_tests)
    add_test(NAME image_pck_n COMMAND image_pck_n_tests)
    if(WIN32)
        set_tests_properties(image_pck_n PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(image_msaa_load_tests ${LIBS_INCLUDE_DIR}/tests/ImageMsaaLoad.cpp)
    target_link_libraries(image_msaa_load_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(image_msaa_load_tests)
    add_test(NAME image_msaa_load COMMAND image_msaa_load_tests)
    if(WIN32)
        set_tests_properties(image_msaa_load PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
