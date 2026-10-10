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
    anyps5_add_library_test(agc_write_watch_coverage_tests
        LABEL unit
        SOURCES tests/unit/WriteWatchCoverage.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_profile_output_tests
        TIMEOUT 20
        SOURCES tests/integration/ProfileOutput.cpp
        LINK libc
    )
endif()

if(WIN32)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0602)
endif()

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()

set(agcTestProperties ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set(agcTestIncludes
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)

function(agc_add_test_variant target name label)
    cmake_parse_arguments(PARSE_ARGV 3 variant "" "" "ARGS;PROPERTIES")
    anyps5_default_test_timeout(${label} variantTimeout)
    add_test(NAME ${name} COMMAND ${target} ${variant_ARGS})
    anyps5_set_test_properties(${name} ${label} "${variantTimeout}" SKIP_RETURN_CODE 77 ${variant_PROPERTIES})
    if(WIN32)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endfunction()

function(agc_add_spirv_mode_tests target)
    string(REGEX REPLACE "_tests$" "" baseName "${target}")
    if(WIN32)
        set_tests_properties(${baseName} PROPERTIES ENVIRONMENT_MODIFICATION
                "APS5_SPIRV_OPT=unset:;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    else()
        set_tests_properties(${baseName} PROPERTIES ENVIRONMENT_MODIFICATION "APS5_SPIRV_OPT=unset:")
    endif()
    foreach(spirvMode IN ITEMS full none)
        agc_add_test_variant(${target} ${baseName}_${spirvMode} unit)
        set_tests_properties(${baseName}_${spirvMode} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SPIRV_OPT=${spirvMode}" TIMEOUT 60)
    endforeach()
endfunction()

if(BUILD_TESTING)
    anyps5_add_library_test(agc_driver_tests
        LABEL gpu
        SOURCES tests/gpu/Submit.cpp
        LINK libSceAgcDriver libkernel libc
        RUNTIME libc
        INCLUDES ${agcTestIncludes}
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_shader_memory_tests
        SOURCES tests/integration/ShaderMemory.cpp Execution/src/ShaderMemory.cpp Execution/src/GuestMemory.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )
    agc_add_test_variant(agc_shader_memory_tests agc_shader_bindless integration
        ARGS bindless
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_shared_write_tests
        SOURCES tests/integration/SharedWrites.cpp
        LINK libSceAgcDriver libkernel libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_write_tracking_tests
        SOURCES tests/integration/WriteTracking.cpp Execution/src/GuestMemory.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
    )

    anyps5_add_library_test(agc_structurizer_tests
        LABEL unit
        SOURCES tests/unit/Structurizer.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_fragment_barycentric_tests
        LABEL unit
        SOURCES tests/unit/FragmentBarycentric.cpp
        LINK libc
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
    )

    anyps5_add_library_test(agc_subgroup_clock_tests
        LABEL unit
        SOURCES tests/unit/SubgroupClock.cpp Execution/src/SubgroupClock.cpp
        LINK libc
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
    )

    anyps5_add_library_test(agc_tessellation_factor_ring_tests
        SOURCES tests/integration/TessellationFactorRing.cpp State/src/Configuration.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_spirv_module_tests
        LABEL unit
        SOURCES tests/unit/SpirvModule.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_spirv_narrow_constant_store_tests
        LABEL unit
        TIMEOUT 60
        SOURCES tests/unit/SpirvNarrowConstantStore.cpp
        LINK shader_recompiler_agc_driver libc $<$<BOOL:${ANYPS5_ENABLE_SPIRV_TOOLS}>:SPIRV-Tools-static>
        RUNTIME libc
        PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
    )
    agc_add_spirv_mode_tests(agc_spirv_narrow_constant_store_tests)

    if(ANYPS5_ENABLE_SPIRV_TOOLS)
        anyps5_add_library_test(agc_spirv_optimizer_tests
            LABEL unit
            TIMEOUT 60
            SOURCES tests/unit/SpirvOptimizer.cpp
            LINK shader_recompiler_agc_driver libc SPIRV-Tools-static
            RUNTIME libc
            PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
        )
        agc_add_spirv_mode_tests(agc_spirv_optimizer_tests)
    endif()

    anyps5_add_library_test(agc_float16_conversion_tests
        LABEL unit
        SOURCES tests/unit/Float16Conversion.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
    )

    anyps5_add_library_test(agc_dpp_write_tests
        LABEL unit
        SOURCES tests/unit/DppWrite.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_masked_select_tests
        LABEL unit
        SOURCES tests/unit/MaskedSelect.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_wave_control_decode_tests
        LABEL unit
        SOURCES tests/unit/WaveControlDecode.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
    )

    anyps5_add_library_test(agc_vop1_control_decode_tests
        LABEL unit
        SOURCES tests/unit/Vop1ControlDecode.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_dcc_key_sharing_tests
        LABEL unit
        SOURCES tests/unit/DccKeySharing.cpp
        LINK libc
        INCLUDES ${agcTestIncludes}
    )

    anyps5_add_library_test(agc_buffer_pool_tests
        SOURCES tests/integration/BufferPool.cpp
        LINK libSceAgcDriver libc
        RUNTIME libc
        INCLUDES ${agcTestIncludes}
    )

    anyps5_add_library_test(agc_display_buffer_dcc_tests
        SOURCES tests/integration/DisplayBufferDcc.cpp
        LINK libSceAgcDriver libc
        RUNTIME libc
        INCLUDES ${agcTestIncludes}
    )

    anyps5_add_library_test(agc_validate_program_tests
        LABEL unit
        SOURCES tests/unit/ValidateProgram.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_shader_disk_cache_tests
        CUSTOM_MAIN
        SOURCES tests/integration/ShaderDiskCache.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_shader_device_profile_tests
        LABEL unit
        SOURCES tests/unit/ShaderDeviceProfile.cpp Execution/src/ShaderDeviceProfile.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_shader_control_flow_tests
        LABEL unit
        SOURCES tests/unit/ControlFlow.cpp
        LINK shader_recompiler_agc_driver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_driver_flip_tests
        SOURCES tests/integration/Flip.cpp
        LINK libSceAgcDriver libc
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )
    agc_add_test_variant(agc_driver_flip_tests agc_driver_flip_compute integration
        ARGS compute
        PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
    )

    foreach(agcTest IN ITEMS agc_driver_tests agc_shader_memory_tests agc_shared_write_tests agc_write_tracking_tests
            agc_structurizer_tests agc_fragment_barycentric_tests agc_subgroup_clock_tests agc_tessellation_factor_ring_tests
            agc_spirv_module_tests agc_spirv_narrow_constant_store_tests agc_float16_conversion_tests agc_dpp_write_tests
            agc_masked_select_tests agc_wave_control_decode_tests agc_vop1_control_decode_tests agc_dcc_key_sharing_tests
            agc_buffer_pool_tests agc_display_buffer_dcc_tests agc_validate_program_tests agc_shader_disk_cache_tests
            agc_shader_device_profile_tests agc_shader_control_flow_tests agc_driver_flip_tests)
        set_target_properties(${agcTest} PROPERTIES CXX_EXTENSIONS OFF)
    endforeach()
    if(TARGET agc_spirv_optimizer_tests)
        set_target_properties(agc_spirv_optimizer_tests PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()

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

if(BUILD_TESTING)
    anyps5_add_library_test(agc_driver_recorder_tests
        NAME agc_pipeline_cache
        LABEL gpu
        SOURCES
            tests/gpu/Recorder.cpp
            tests/gpu/ResidentPresent.cpp
            ${agcTriangleHeaders}
            ${agcSampleLodShaderHeader}
            ${agcSampleArrayShaderHeader}
            ${agcCoverShaderHeaders}
        LINK libkernel libSceAgcDriver SDL2-static libc
        RUNTIME libc
        INCLUDES
            ${agcTestShaderGeneratedRoot}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ARGS --case CachedPipeline_Keys_RetainIdenticalPipelinesAndSeparateChangedOnes
    )

    anyps5_add_library_test(agc_recorder_kept_bytes_tests
        SOURCES tests/integration/RecorderKeptBytes.cpp
        LINK libSceAgcDriver libc
        RUNTIME libc
        INCLUDES
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
    )

    anyps5_add_library_test(agc_driver_pm4_tests
        SOURCES tests/integration/Pm4.cpp
        LINK libSceAgcDriver libc libkernel
        RUNTIME libc
        PROPERTIES ${agcTestProperties}
    )
    agc_add_test_variant(agc_driver_pm4_tests agc_driver_pm4_failure integration
        ARGS failure
        PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
    )

    anyps5_add_library_test(agc_driver_graphics_tests
        CUSTOM_MAIN
        SOURCES
            tests/integration/Graphics.cpp
            tests/integration/ColorTargetLayout.cpp
            tests/integration/BdaResources.cpp
            tests/integration/GuestAllocations.cpp
            tests/integration/LiveStack.cpp
            tests/integration/TextureFormat.cpp
            tests/integration/TextureTiling.cpp
            tests/integration/GuestTextureResource.cpp
            tests/integration/GuestSamplerResource.cpp
            tests/integration/TextureDetiler.cpp
            tests/integration/DepthSurfaceReuse.cpp
            Graphics/src/Draw.cpp
            Graphics/src/DepthSurface.cpp
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
            ${agcMeshArgumentsShaderHeader}
            ${agcSampleCounterShaderHeader}
            ${agcSampleDumpsShaderHeader}
            Execution/src/GuestMemory.cpp
            Execution/src/Pm4.cpp
        LINK shader_recompiler_agc_driver libc libkernel
        RUNTIME libc
        INCLUDES
            ${agcTextureDetileGeneratedRoot}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_driver_bda_device_tests
        LABEL gpu
        SOURCES
            tests/gpu/BdaDevice.cpp
            tests/gpu/BdaShader.cpp
            tests/gpu/BdaExecution.cpp
            tests/gpu/BdaContracts.cpp
            Execution/src/BdaFeatures.cpp
            Graphics/src/Resources.cpp
            Graphics/src/BufferAddress.cpp
        LINK SDL2-static libc shader_recompiler_agc_driver
        RUNTIME libc
        INCLUDES
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        PROPERTIES ${agcTestProperties}
    )

    anyps5_add_library_test(agc_driver_mesh_tests
        LABEL gpu
        SOURCES tests/gpu/Mesh.cpp ${agcDriverSources}
        LINK shader_recompiler_agc_driver SDL2-static libc libkernel
        RUNTIME libc
        INCLUDES
            ${agcTextureDetileGeneratedRoot}
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
        PROPERTIES ${agcTestProperties}
    )

    foreach(agcTest IN ITEMS agc_driver_recorder_tests agc_recorder_kept_bytes_tests agc_driver_pm4_tests
            agc_driver_graphics_tests agc_driver_bda_device_tests agc_driver_mesh_tests)
        set_target_properties(${agcTest} PROPERTIES CXX_EXTENSIONS OFF)
    endforeach()
endif()

file(GLOB agcExecutionTestSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/*.cpp)
set(agcExecutionTests)
foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)
    get_filename_component(agcExecutionTestName ${agcExecutionTestSource} NAME_WE)
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" agcExecutionTestName "${agcExecutionTestName}")
    string(TOLOWER "agc_driver_${agcExecutionTestName}_tests" agcExecutionTest)
    if(NOT BUILD_TESTING)
        add_test_executable(${agcExecutionTest} EXCLUDE_FROM_ALL ${agcExecutionTestSource})
        target_link_libraries(${agcExecutionTest} PRIVATE libSceAgcDriver SDL2-static libc)
        configure_windows_unwind(${agcExecutionTest})
    elseif(agcExecutionTest STREQUAL "agc_driver_constant_store_tests")
        add_test_executable(${agcExecutionTest} EXCLUDE_FROM_ALL ${agcExecutionTestSource} ${ANYPS5_TESTING_MAIN})
        target_link_libraries(${agcExecutionTest} PRIVATE anyps5_testing libSceAgcDriver SDL2-static libc)
        configure_windows_unwind(${agcExecutionTest})
    else()
        file(READ ${agcExecutionTestSource} agcExecutionTestText)
        set(agcExecutionTestMain)
        if(agcExecutionTestText MATCHES "int main[(]")
            set(agcExecutionTestMain CUSTOM_MAIN)
        endif()
        anyps5_add_library_test(${agcExecutionTest}
            ${agcExecutionTestMain}
            LABEL gpu
            SOURCES ${agcExecutionTestSource}
            LINK libSceAgcDriver SDL2-static libc
            RUNTIME libc
            PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
    target_include_directories(${agcExecutionTest} PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
    )
    set_target_properties(${agcExecutionTest} PROPERTIES CXX_EXTENSIONS OFF)
    list(APPEND agcExecutionTests ${agcExecutionTest})
endforeach()

target_include_directories(agc_driver_prepared_exports_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_publication_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_graphics_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(agc_driver_interpolation_modes_tests PRIVATE shader_recompiler_agc_driver)

target_include_directories(agc_driver_prepared_shaders_tests PRIVATE
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
)

if(BUILD_TESTING)
    set_tests_properties(agc_driver_flat_store PROPERTIES RUN_SERIAL TRUE)
    set_tests_properties(agc_driver_storage_cache_budget PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sampled_cache_budget_memory COMMAND agc_driver_sampled_cache_budget_tests memory)
    set_tests_properties(agc_driver_sampled_cache_budget_memory PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_sampled_cache_budget_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    foreach(agcTest IN ITEMS agc_driver_constant_store_tests)
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
                    LABELS gpu ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 60 SKIP_RETURN_CODE 77)
            if(agcTest STREQUAL "agc_driver_constant_store_tests")
                set_tests_properties(${agcTestName} PROPERTIES RUN_SERIAL TRUE)
            endif()
            if(WIN32)
                set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    foreach(preparedMode IN ITEMS indirect uncached debug debug_failure deferred)
        set(preparedTest "agc_driver_prepared_shaders_${preparedMode}")
        if(preparedMode STREQUAL "indirect")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --indirect)
        elseif(preparedMode STREQUAL "deferred")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --deferred)
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
endif()

option(AGC_BUILD_VISUAL_TEST "Enable the standalone AGC SPIR-V visual test" OFF)

if(AGC_BUILD_VISUAL_TEST AND NOT BUILD_TESTING)
    message(FATAL_ERROR "AGC_BUILD_VISUAL_TEST needs BUILD_TESTING for the test framework")
endif()

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
            tests/gpu/Visual.cpp
            ${ANYPS5_TESTING_MAIN}
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
            anyps5_testing
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

if(BUILD_TESTING)
    foreach(agcDecodeTest IN ITEMS RdnaTruncatedModifier MubufLdsDecode Vop1SdwaMoveDecode PixelPositionExport)
        string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" agcDecodeTarget "${agcDecodeTest}")
        string(TOLOWER "agc_${agcDecodeTarget}_tests" agcDecodeTarget)
        anyps5_add_library_test(${agcDecodeTarget}
            LABEL unit
            SOURCES tests/unit/${agcDecodeTest}.cpp
            LINK shader_recompiler_agc_driver libc
            RUNTIME libc
        )
        set_target_properties(${agcDecodeTarget} PROPERTIES CXX_EXTENSIONS OFF)
    endforeach()
    set_tests_properties(agc_pixel_position_export PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
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
