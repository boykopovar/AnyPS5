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
set(agcTextureDetileImageShaderSpv ${agcTextureDetileGeneratedDir}/TextureDetileImage.spv)
set(agcTextureDetileImageShaderHeader ${agcTextureDetileGeneratedDir}/TextureDetileImage_spv.h)
add_custom_command(
        OUTPUT "${agcTextureDetileImageShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -DLINEAR_IMAGE=1 -o "${agcTextureDetileImageShaderSpv}" "${agcTextureDetileShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcTextureDetileImageShaderSpv} -DOUTPUT=${agcTextureDetileImageShaderHeader}
        -DSYMBOL=TEXTURE_DETILE_IMAGE_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcTextureDetileShaderSource}" glslang-standalone
        VERBATIM
)

set(agcMeshArgumentsShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MeshArguments.comp)
foreach(agcReadbackKind IN ITEMS Uint Depth)
    string(TOUPPER ${agcReadbackKind} agcReadbackSymbol)
    if(agcReadbackKind STREQUAL "Depth")
        set(agcReadbackDepth 1)
    else()
        set(agcReadbackDepth 0)
    endif()
    set(agcReadbackSpv ${agcTextureDetileGeneratedDir}/MultisampleReadback${agcReadbackKind}.spv)
    set(agcReadbackHeader ${agcTextureDetileGeneratedDir}/MultisampleReadback${agcReadbackKind}_spv.h)
    add_custom_command(
            OUTPUT "${agcReadbackHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -DREAD_DEPTH=${agcReadbackDepth} -o "${agcReadbackSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MultisampleSampledReadback.comp"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcReadbackSpv} -DOUTPUT=${agcReadbackHeader}
            -DSYMBOL=MULTISAMPLE_READBACK_${agcReadbackSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MultisampleSampledReadback.comp" glslang-standalone
            VERBATIM
    )
    list(APPEND agcReadbackHeaders ${agcReadbackHeader})
endforeach()

set(agcMultisampleUploadSpv ${agcTextureDetileGeneratedDir}/MultisampleUpload.spv)
set(agcMultisampleUploadHeader ${agcTextureDetileGeneratedDir}/MultisampleUpload_spv.h)
add_custom_command(
        OUTPUT "${agcMultisampleUploadHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcMultisampleUploadSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MultisampleUpload.comp"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcMultisampleUploadSpv} -DOUTPUT=${agcMultisampleUploadHeader}
        -DSYMBOL=MULTISAMPLE_UPLOAD_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/MultisampleUpload.comp" glslang-standalone
        VERBATIM
)

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

set(agcTextureRepackShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/TextureRepack.comp)
set(agcTextureRepackShaderSpv ${agcTextureDetileGeneratedDir}/TextureRepack.spv)
set(agcTextureRepackShaderHeader ${agcTextureDetileGeneratedDir}/TextureRepack_spv.h)
add_custom_command(
        OUTPUT "${agcTextureRepackShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcTextureRepackShaderSpv}" "${agcTextureRepackShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcTextureRepackShaderSpv} -DOUTPUT=${agcTextureRepackShaderHeader}
        -DSYMBOL=TEXTURE_REPACK_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcTextureRepackShaderSource}" glslang-standalone
        VERBATIM
)

set(agcDispatchArgumentsShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/DispatchArguments.comp)
set(agcDispatchArgumentsShaderSpv ${agcTextureDetileGeneratedDir}/DispatchArguments.spv)
set(agcDispatchArgumentsShaderHeader ${agcTextureDetileGeneratedDir}/DispatchArguments_spv.h)
add_custom_command(
        OUTPUT "${agcDispatchArgumentsShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcDispatchArgumentsShaderSpv}" "${agcDispatchArgumentsShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcDispatchArgumentsShaderSpv} -DOUTPUT=${agcDispatchArgumentsShaderHeader}
        -DSYMBOL=DISPATCH_ARGUMENTS_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcDispatchArgumentsShaderSource}" glslang-standalone
        VERBATIM
)

set(agcDepthSampleShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/DepthSample.comp)
foreach(agcDepthSampleKind IN ITEMS Single Multisample)
    string(TOUPPER ${agcDepthSampleKind} agcDepthSampleSymbol)
    if(agcDepthSampleKind STREQUAL "Multisample")
        set(agcDepthSampleMultisampled 1)
    else()
        set(agcDepthSampleMultisampled 0)
    endif()
    set(agcDepthSampleShaderSpv ${agcTextureDetileGeneratedDir}/DepthSample${agcDepthSampleKind}.spv)
    set(agcDepthSampleShaderHeader ${agcTextureDetileGeneratedDir}/DepthSample${agcDepthSampleKind}_spv.h)
    add_custom_command(
            OUTPUT "${agcDepthSampleShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -DMULTISAMPLED=${agcDepthSampleMultisampled} -o "${agcDepthSampleShaderSpv}" "${agcDepthSampleShaderSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcDepthSampleShaderSpv} -DOUTPUT=${agcDepthSampleShaderHeader}
            -DSYMBOL=DEPTH_SAMPLE_${agcDepthSampleSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcDepthSampleShaderSource}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcDepthSampleShaderHeaders ${agcDepthSampleShaderHeader})
endforeach()

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
foreach(agcPresentationStage IN ITEMS Vert Frag)
    string(TOLOWER "${agcPresentationStage}" agcPresentationExtension)
    string(TOUPPER "${agcPresentationStage}" agcPresentationSymbol)
    set(agcPresentationShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/PresentationPass.${agcPresentationExtension})
    set(agcPresentationShaderSpv ${agcTextureDetileGeneratedDir}/PresentationPass${agcPresentationStage}.spv)
    set(agcPresentationShaderHeader ${agcTextureDetileGeneratedDir}/PresentationPass${agcPresentationStage}_spv.h)
    add_custom_command(
            OUTPUT "${agcPresentationShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcPresentationShaderSpv}" "${agcPresentationShaderSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcPresentationShaderSpv} -DOUTPUT=${agcPresentationShaderHeader}
            -DSYMBOL=PRESENTATION_PASS_${agcPresentationSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcPresentationShaderSource}" "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/PqToSdr.glsl" glslang-standalone
            VERBATIM
    )
    list(APPEND agcPresentationShaderHeaders ${agcPresentationShaderHeader})
endforeach()
foreach(agcSampleOrderVariant IN ITEMS "R8;comp;-DTEXEL_FORMAT=r8ui" "R16;comp;-DTEXEL_FORMAT=r16ui" "R32;comp;-DTEXEL_FORMAT=r32ui" "RG32;comp;-DTEXEL_FORMAT=rg32ui" "RGBA32;comp;-DTEXEL_FORMAT=rgba32ui" "Vertex;vert;-DPLAIN=1" "Depth;frag;-DPLAIN=1" "StencilExport;frag;-DSTENCIL_EXPORT=1" "StencilBits;frag;-DSTENCIL_BITS=1")
    list(GET agcSampleOrderVariant 0 agcSampleOrderName)
    list(GET agcSampleOrderVariant 1 agcSampleOrderExtension)
    list(GET agcSampleOrderVariant 2 agcSampleOrderDefine)
    string(TOUPPER "${agcSampleOrderName}" agcSampleOrderSymbol)
    set(agcSampleOrderShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/SampleOrder.${agcSampleOrderExtension})
    set(agcSampleOrderShaderSpv ${agcTextureDetileGeneratedDir}/SampleOrder${agcSampleOrderName}.spv)
    set(agcSampleOrderShaderHeader ${agcTextureDetileGeneratedDir}/SampleOrder${agcSampleOrderName}_spv.h)
    add_custom_command(
            OUTPUT "${agcSampleOrderShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 ${agcSampleOrderDefine} -o "${agcSampleOrderShaderSpv}" "${agcSampleOrderShaderSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleOrderShaderSpv} -DOUTPUT=${agcSampleOrderShaderHeader}
            -DSYMBOL=SAMPLE_ORDER_${agcSampleOrderSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcSampleOrderShaderSource}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcSampleOrderShaderHeaders ${agcSampleOrderShaderHeader})
endforeach()
foreach(agcFirstSampleClass IN ITEMS Float Uint Sint)
    string(TOUPPER "${agcFirstSampleClass}" agcFirstSampleSymbol)
    set(agcFirstSampleShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/FirstSample.frag)
    set(agcFirstSampleShaderSpv ${agcTextureDetileGeneratedDir}/FirstSample${agcFirstSampleClass}.spv)
    set(agcFirstSampleShaderHeader ${agcTextureDetileGeneratedDir}/FirstSample${agcFirstSampleClass}_spv.h)
    add_custom_command(
            OUTPUT "${agcFirstSampleShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -D${agcFirstSampleSymbol}_SAMPLES=1 -o "${agcFirstSampleShaderSpv}" "${agcFirstSampleShaderSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcFirstSampleShaderSpv} -DOUTPUT=${agcFirstSampleShaderHeader}
            -DSYMBOL=FIRST_SAMPLE_${agcFirstSampleSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcFirstSampleShaderSource}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcPresentationShaderHeaders ${agcFirstSampleShaderHeader})
endforeach()
foreach(agcOverlayStage IN ITEMS Vert Frag)
    string(TOLOWER "${agcOverlayStage}" agcOverlayExtension)
    string(TOUPPER "${agcOverlayStage}" agcOverlaySymbol)
    set(agcOverlayShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/PerformanceOverlay.${agcOverlayExtension})
    set(agcOverlayShaderSpv ${agcTextureDetileGeneratedDir}/PerformanceOverlay${agcOverlayStage}.spv)
    set(agcOverlayShaderHeader ${agcTextureDetileGeneratedDir}/PerformanceOverlay${agcOverlayStage}_spv.h)
    add_custom_command(
            OUTPUT "${agcOverlayShaderHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcOverlayShaderSpv}" "${agcOverlayShaderSource}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcOverlayShaderSpv} -DOUTPUT=${agcOverlayShaderHeader}
            -DSYMBOL=PERFORMANCE_OVERLAY_${agcOverlaySymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${agcOverlayShaderSource}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcPresentationShaderHeaders ${agcOverlayShaderHeader})
endforeach()

set(agcDriverSources
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ComputePipelineCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/DisplayBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/DisplayFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/HostHdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/DeviceAccess.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Diagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DataHitVerification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchAhead.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchCacheStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchInsert.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchLookup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/DispatchTiming.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/IndirectDispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Dispatch/VariantValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawProgramTrace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCacheStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawDecode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawLookup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawPipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawPrecheck.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawRegisterKey.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Draw/DrawRelocate.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Queues/SubmissionRing.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Queues/WorkerAffinity.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Shaders/ShaderDiagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Shaders/ShaderRegistry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/DeferredLabels.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/LabelHistory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/SynchronizationStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/WaitMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Synchronization/WaitStatistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/WorkerSampler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/HostAllocator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PerformanceTrace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PerformanceTraceThread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/TraceFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ThreadPriority.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ShaderMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/BdaFeatures.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/SubgroupClock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/ShaderDeviceProfile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Pm4.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/SubmissionCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/FrameCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/FrameRecorder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/VulkanDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/FloatFlushProbe.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/AspectFit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PresentationScaler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PresentationPass.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PerformanceOverlay.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Screenshot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/DepthSurface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/MultisampleTarget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/FirstSampleCopy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ColorTargetLayout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ColorTargetTransfer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PipelineLibrary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PipelineCache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PipelineJournal.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PlaceholderImages.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Recorder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ParallelCompare.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderInputState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderAddressResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestBufferMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ResidentMirror.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/BdaResources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/BufferAddress.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/ShaderValidation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/State.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestTextureResource.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/GuestSamplerResource.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureFormat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/DccMetadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureTiling.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/MultisampleTextureLayout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/TextureDetiler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/MultisampleReadback.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/MultisampleUpload.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Texture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/PassHazards.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/SampleOrder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/BarrierBatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/UnitShadow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Sampler.cpp
        ${agcSampleOrderShaderHeaders}
        ${agcTextureDetileShaderHeader}
        ${agcReadbackHeaders}
        ${agcMultisampleUploadHeader}
        ${agcCmaskClearShaderHeader}
        ${agcDepthSampleShaderHeaders}
        ${agcMeshArgumentsShaderHeader}
        ${agcTextureDetileImageShaderHeader}
        ${agcTextureRepackShaderHeader}
        ${agcDispatchArgumentsShaderHeader}
        ${agcSampleCounterShaderHeader}
        ${agcSampleDumpsShaderHeader}
        ${agcPresentationShaderHeaders}
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

find_package(Git QUIET)
set(agcTraceRevisionHeader ${agcTextureDetileGeneratedRoot}/prx/libSceAgcDriver/TraceRevision.hpp)
add_custom_target(agc_trace_revision
        COMMAND ${CMAKE_COMMAND} -DGIT=${GIT_EXECUTABLE} -DSOURCE=${CMAKE_SOURCE_DIR} -DOUTPUT=${agcTraceRevisionHeader} -P ${CMAKE_CURRENT_SOURCE_DIR}/trace_revision.cmake
        BYPRODUCTS ${agcTraceRevisionHeader}
        VERBATIM
)
set(agcZlibIncludeDir ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/TraceFile.cpp PROPERTIES
        INCLUDE_DIRECTORIES ${agcZlibIncludeDir}
)

add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL
        ${agcDriverSources}
)
add_dependencies(libSceAgcDriver agc_trace_revision)

target_include_directories(libSceAgcDriver PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
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
        decoder_png
        SDL2-static
        libc
        libkernel
        jpeg_turbo
        libzstd_static
        ${CMAKE_DL_LIBS}
)
if(APS5_ENABLE_TRACY)
    target_link_libraries(libSceAgcDriver PUBLIC TracyClient)
endif()
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceAgcDriver PRIVATE LINKER:--exclude-libs,$<TARGET_FILE_NAME:decoder_png>)
endif()

if(BUILD_TESTING)
    add_test_executable(agc_driver_unsupported_diagnostics_tests tests/UnsupportedDiagnostics.cpp)
    target_include_directories(agc_driver_unsupported_diagnostics_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_unsupported_diagnostics_tests PRIVATE libSceAgcDriver)
    configure_windows_unwind(agc_driver_unsupported_diagnostics_tests)
    add_test(NAME agc_driver_unsupported_diagnostics COMMAND agc_driver_unsupported_diagnostics_tests)
    if(WIN32)
        set_tests_properties(agc_driver_unsupported_diagnostics PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>")
    endif()

    add_test_executable(agc_driver_draw_program_trace_tests
            tests/DrawProgramTrace.cpp
            Execution/src/Driver/Draw/DrawProgramTrace.cpp
    )
    target_include_directories(agc_driver_draw_program_trace_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
    )
    set_target_properties(agc_driver_draw_program_trace_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_draw_program_trace_tests)
    add_test(NAME agc_driver_draw_program_trace COMMAND agc_driver_draw_program_trace_tests)
    set_tests_properties(agc_driver_draw_program_trace PROPERTIES TIMEOUT 20)
    if(MINGW)
        target_link_libraries(agc_driver_draw_program_trace_tests PRIVATE libc)
        set_tests_properties(agc_driver_draw_program_trace PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

set_target_properties(libSceAgcDriver PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceAgcDriver)
if(WIN32)
    target_link_options(libSceAgcDriver PRIVATE LINKER:--exclude-symbols=_Znwy:_Znay:_ZnwyRKSt9nothrow_t:_ZnayRKSt9nothrow_t:_ZdlPv:_ZdaPv:_ZdlPvy:_ZdaPvy:_ZdlPvRKSt9nothrow_t:_ZdaPvRKSt9nothrow_t)
endif()

if(BUILD_TESTING)
    add_test_executable(agc_driver_nop_data_packet_tests tests/NopDataWritePacket.cpp)
    target_include_directories(agc_driver_nop_data_packet_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_nop_data_packet_tests PRIVATE libSceAgcDriver)
    configure_windows_unwind(agc_driver_nop_data_packet_tests)
    add_test(NAME agc_driver_nop_data_packet COMMAND agc_driver_nop_data_packet_tests)
    if(WIN32)
        set_tests_properties(agc_driver_nop_data_packet PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_frame_dump_io_tests tests/FrameDumpIo.cpp)
    target_include_directories(agc_frame_dump_io_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_frame_dump_io_tests PRIVATE libc)
    configure_windows_unwind(agc_frame_dump_io_tests)
    add_test(NAME agc_frame_dump_io COMMAND agc_frame_dump_io_tests)
    set_tests_properties(agc_frame_dump_io PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_frame_dump_io PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_frame_capture_tests tests/FrameCapture.cpp Execution/src/FrameCapture.cpp)
    target_include_directories(agc_frame_capture_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_frame_capture_tests PRIVATE libc libzstd_static)
    set_target_properties(agc_frame_capture_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_frame_capture_tests)
    add_test(NAME agc_frame_capture COMMAND agc_frame_capture_tests)
    set_tests_properties(agc_frame_capture PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_frame_capture PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

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

    add_test_executable(agc_async_log_tests tests/AsyncLog.cpp)
    target_include_directories(agc_async_log_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_async_log_tests PRIVATE libc)
    configure_windows_unwind(agc_async_log_tests)
    add_test(NAME agc_async_log COMMAND agc_async_log_tests)
    add_test(NAME agc_async_log_fatal COMMAND agc_async_log_tests fatal)
    set_tests_properties(agc_async_log agc_async_log_fatal PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_async_log agc_async_log_fatal PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_stack_walk_tests tests/StackWalk.cpp)
    target_include_directories(agc_stack_walk_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_stack_walk_tests PRIVATE libc)
    configure_windows_unwind(agc_stack_walk_tests)
    add_test(NAME agc_stack_walk COMMAND agc_stack_walk_tests)
    set_tests_properties(agc_stack_walk PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_stack_walk PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_performance_trace_tests tests/PerformanceTrace.cpp Execution/src/TraceFile.cpp)
    target_include_directories(agc_performance_trace_tests PRIVATE ${LIBS_INCLUDE_DIR} ${agcZlibIncludeDir})
    target_link_libraries(agc_performance_trace_tests PRIVATE libc jpeg_turbo)
    configure_windows_unwind(agc_performance_trace_tests)
    add_test(NAME agc_performance_trace COMMAND agc_performance_trace_tests)
    set_tests_properties(agc_performance_trace PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_performance_trace PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_handoff_spin_tests tests/HandoffSpin.cpp)
    target_include_directories(agc_handoff_spin_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_handoff_spin_tests PRIVATE libc)
    configure_windows_unwind(agc_handoff_spin_tests)
    add_test(NAME agc_handoff_spin COMMAND agc_handoff_spin_tests)
    set_tests_properties(agc_handoff_spin PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_handoff_spin PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(WIN32)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Lifecycle.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0602)
endif()

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()

add_test_executable(agc_driver_register_key_tests EXCLUDE_FROM_ALL tests/RegisterKey.cpp)
target_include_directories(agc_driver_register_key_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_register_key_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)
set_target_properties(agc_driver_register_key_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_register_key_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_register_key_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_register_key COMMAND agc_driver_register_key_tests)
    set_tests_properties(agc_driver_register_key PROPERTIES TIMEOUT 30 ENVIRONMENT "APS5_VERIFY_REGISTER_KEYS=1")
    if(WIN32)
        set_tests_properties(agc_driver_register_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING AND WIN32)
    add_test_executable(agc_driver_host_allocator_tests tests/HostAllocator.cpp Execution/src/HostAllocator.cpp)
    target_link_libraries(agc_driver_host_allocator_tests PRIVATE libc)
    configure_windows_unwind(agc_driver_host_allocator_tests)
    add_test(NAME agc_driver_host_allocator COMMAND agc_driver_host_allocator_tests)
    set_tests_properties(agc_driver_host_allocator PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()

add_test_executable(agc_driver_key_miss_tests EXCLUDE_FROM_ALL tests/KeyMiss.cpp)
target_include_directories(agc_driver_key_miss_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_key_miss_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)
set_target_properties(agc_driver_key_miss_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_key_miss_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_key_miss_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_key_miss COMMAND agc_driver_key_miss_tests)
    set_tests_properties(agc_driver_key_miss PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_key_miss PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_driver_tests EXCLUDE_FROM_ALL
        tests/Submit.cpp
)

target_include_directories(agc_driver_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_tests PRIVATE
        libSceAgcDriver
        libkernel
        libc
)

set_target_properties(agc_driver_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_tests)

add_test_executable(agc_driver_wait_shutdown_tests EXCLUDE_FROM_ALL tests/WaitShutdown.cpp)
target_include_directories(agc_driver_wait_shutdown_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_driver_wait_shutdown_tests PRIVATE libSceAgcDriver libkernel libc)
set_target_properties(agc_driver_wait_shutdown_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_wait_shutdown_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_wait_shutdown_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_wait_shutdown COMMAND agc_driver_wait_shutdown_tests)
    set_tests_properties(agc_driver_wait_shutdown PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_WAIT_TIMEOUT_MS=20"
            TIMEOUT 10 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_resolution_shutdown COMMAND agc_driver_wait_shutdown_tests --resolution-shutdown)
    set_tests_properties(agc_driver_resolution_shutdown PROPERTIES TIMEOUT 10 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_wait_shutdown agc_driver_resolution_shutdown PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_shader_memory_tests EXCLUDE_FROM_ALL
        tests/ShaderMemory.cpp
        Execution/src/ShaderMemory.cpp
        Execution/src/GuestMemory.cpp
        Execution/src/PerformanceTraceThread.cpp
)
target_include_directories(agc_shader_memory_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_memory_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_memory_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_memory_tests)

add_test_executable(agc_vertex_input_tests EXCLUDE_FROM_ALL tests/VertexInput.cpp)
target_include_directories(agc_vertex_input_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(agc_vertex_input_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_input_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_input_tests)

add_test_executable(agc_guarded_memory_loads_tests EXCLUDE_FROM_ALL tests/GuardedMemoryLoads.cpp)
target_link_libraries(agc_guarded_memory_loads_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_guarded_memory_loads_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_guarded_memory_loads_tests)
if(BUILD_TESTING)
    set_target_properties(agc_guarded_memory_loads_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_guarded_memory_loads COMMAND agc_guarded_memory_loads_tests)
    set_tests_properties(agc_guarded_memory_loads PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_guarded_memory_loads PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_lds_lock_budget_tests EXCLUDE_FROM_ALL tests/LdsLockBudget.cpp)
target_link_libraries(agc_lds_lock_budget_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_lds_lock_budget_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_lds_lock_budget_tests)
if(BUILD_TESTING)
    set_target_properties(agc_lds_lock_budget_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_lds_lock_budget COMMAND agc_lds_lock_budget_tests)
    set_tests_properties(agc_lds_lock_budget PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_lds_lock_budget PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_shared_write_tests EXCLUDE_FROM_ALL tests/SharedWrites.cpp)
target_include_directories(agc_shared_write_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shared_write_tests PRIVATE libSceAgcDriver libkernel libc)
set_target_properties(agc_shared_write_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shared_write_tests)

if(BUILD_TESTING AND WIN32)
    add_test_executable(agc_page_state_tests tests/PageStates.cpp)
    target_include_directories(agc_page_state_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_page_state_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(agc_page_state_tests)
    add_test(NAME agc_page_states COMMAND agc_page_state_tests)
    add_test(NAME agc_page_state_metadata COMMAND agc_page_state_tests --metadata)
    set_tests_properties(agc_page_states agc_page_state_metadata PROPERTIES
        TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()

add_test_executable(agc_locks_tests EXCLUDE_FROM_ALL tests/Locks.cpp)
target_include_directories(agc_locks_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_locks_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_locks_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_locks_tests)

add_test_executable(agc_write_tracking_tests EXCLUDE_FROM_ALL
        tests/WriteTracking.cpp
        Execution/src/GuestMemory.cpp
        Execution/src/PerformanceTraceThread.cpp
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
        add_test(NAME agc_write_tracking_metadata_commit COMMAND agc_write_tracking_tests --metadata-commit)
        set_tests_properties(agc_write_tracking_metadata_commit PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
        add_test(NAME agc_resident_direct_remap COMMAND agc_write_tracking_tests --resident-direct-remap)
        set_tests_properties(agc_resident_direct_remap PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        set_tests_properties(agc_write_tracking agc_write_tracking_metadata_commit agc_resident_direct_remap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING AND NOT WIN32)
    add_test_executable(agc_collect_memo_generation_tests
            tests/CollectMemoGeneration.cpp
            Execution/src/GuestMemory.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_collect_memo_generation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_collect_memo_generation_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_collect_memo_generation_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME agc_collect_memo_generation COMMAND agc_collect_memo_generation_tests)
    set_tests_properties(agc_collect_memo_generation PROPERTIES TIMEOUT 30)
endif()

add_test_executable(agc_thread_priority_tests EXCLUDE_FROM_ALL
        tests/ThreadPriority.cpp
        Execution/src/ThreadPriority.cpp
)
target_include_directories(agc_thread_priority_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_thread_priority_tests PRIVATE libc ${CMAKE_DL_LIBS})
set_target_properties(agc_thread_priority_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_thread_priority_tests)
if(BUILD_TESTING)
    set_target_properties(agc_thread_priority_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_thread_priority COMMAND agc_thread_priority_tests)
    if(WIN32)
        set_tests_properties(agc_thread_priority PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    else()
        foreach(mode rt high)
            add_test(NAME agc_thread_priority_refused_${mode} COMMAND agc_thread_priority_tests refused)
            set_tests_properties(agc_thread_priority_refused_${mode} PROPERTIES ENVIRONMENT
                "APS5_THREAD_PRIORITY=${mode};DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/aps5-no-rtkit")
        endforeach()
    endif()
endif()

add_test_executable(agc_structurizer_tests EXCLUDE_FROM_ALL tests/Structurizer.cpp)
target_link_libraries(agc_structurizer_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_structurizer_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_structurizer_tests)

add_test_executable(agc_fragment_barycentric_tests EXCLUDE_FROM_ALL tests/FragmentBarycentric.cpp)
target_include_directories(agc_fragment_barycentric_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_fragment_barycentric_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_fragment_barycentric_tests)
if(BUILD_TESTING)
    set_target_properties(agc_fragment_barycentric_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_fragment_barycentric COMMAND agc_fragment_barycentric_tests)
endif()

add_test_executable(agc_float32_flush_control_tests EXCLUDE_FROM_ALL tests/Float32FlushControl.cpp)
target_include_directories(agc_float32_flush_control_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_float32_flush_control_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_float32_flush_control_tests)
if(BUILD_TESTING)
    set_target_properties(agc_float32_flush_control_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_float32_flush_control COMMAND agc_float32_flush_control_tests)
endif()

add_test_executable(agc_pipeline_link_optimization_tests EXCLUDE_FROM_ALL tests/PipelineLinkOptimization.cpp)
target_include_directories(agc_pipeline_link_optimization_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_pipeline_link_optimization_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pipeline_link_optimization_tests)
if(BUILD_TESTING)
    set_target_properties(agc_pipeline_link_optimization_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_pipeline_link_optimization COMMAND agc_pipeline_link_optimization_tests)
endif()

add_test_executable(agc_draw_inputs_in_place_driver_tests EXCLUDE_FROM_ALL tests/DrawInputsInPlace.cpp)
target_include_directories(agc_draw_inputs_in_place_driver_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_draw_inputs_in_place_driver_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_draw_inputs_in_place_driver_tests)
if(BUILD_TESTING)
    set_target_properties(agc_draw_inputs_in_place_driver_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_draw_inputs_in_place_driver COMMAND agc_draw_inputs_in_place_driver_tests)
endif()

add_test_executable(agc_subgroup_barrier_tests EXCLUDE_FROM_ALL tests/SubgroupBarrier.cpp)
target_include_directories(agc_subgroup_barrier_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_subgroup_barrier_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_subgroup_barrier_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_subgroup_barrier_tests)
if(BUILD_TESTING)
    set_target_properties(agc_subgroup_barrier_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_subgroup_barrier COMMAND agc_subgroup_barrier_tests)
    set_tests_properties(agc_subgroup_barrier PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_subgroup_barrier PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_device_fault_tests EXCLUDE_FROM_ALL tests/DeviceFault.cpp)
target_include_directories(agc_device_fault_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_device_fault_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(agc_device_fault_tests PRIVATE libc libkernel)
configure_windows_unwind(agc_device_fault_tests)
if(BUILD_TESTING)
    set_target_properties(agc_device_fault_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_device_fault COMMAND agc_device_fault_tests)
    if(WIN32)
        set_tests_properties(agc_device_fault PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_subgroup_clock_tests EXCLUDE_FROM_ALL
        tests/SubgroupClock.cpp
        Execution/src/SubgroupClock.cpp
)
target_include_directories(agc_subgroup_clock_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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
    add_test_executable(agc_spirv13_image_tests EXCLUDE_FROM_ALL tests/Spirv13Image.cpp)
    target_link_libraries(agc_spirv13_image_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_spirv13_image_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_spirv13_image_tests)
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

add_test_executable(agc_f32_denorm_preserve_tests EXCLUDE_FROM_ALL tests/F32DenormPreserve.cpp)
target_link_libraries(agc_f32_denorm_preserve_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_f32_denorm_preserve_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_f32_denorm_preserve_tests)
if(BUILD_TESTING)
    set_target_properties(agc_f32_denorm_preserve_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_f32_denorm_preserve COMMAND agc_f32_denorm_preserve_tests)
    set_tests_properties(agc_f32_denorm_preserve PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_f32_denorm_preserve PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_spirv_branch_targets_tests EXCLUDE_FROM_ALL tests/SpirvBranchTargets.cpp)
target_link_libraries(agc_spirv_branch_targets_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_branch_targets_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_branch_targets_tests)
if(BUILD_TESTING)
    set_target_properties(agc_spirv_branch_targets_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_spirv_branch_targets COMMAND agc_spirv_branch_targets_tests)
    set_tests_properties(agc_spirv_branch_targets PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_spirv_branch_targets PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_spirv_failure_text_tests EXCLUDE_FROM_ALL tests/SpirvFailureText.cpp)
target_link_libraries(agc_spirv_failure_text_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_failure_text_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_failure_text_tests)
if(BUILD_TESTING)
    set_target_properties(agc_spirv_failure_text_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_spirv_failure_text COMMAND agc_spirv_failure_text_tests)
    set_tests_properties(agc_spirv_failure_text PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_spirv_failure_text PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_spirv_lds_atomic_lock_tests EXCLUDE_FROM_ALL tests/SpirvLdsAtomicLock.cpp)
target_link_libraries(agc_spirv_lds_atomic_lock_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_lds_atomic_lock_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_lds_atomic_lock_tests)
if(BUILD_TESTING)
    set_target_properties(agc_spirv_lds_atomic_lock_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_spirv_lds_atomic_lock COMMAND agc_spirv_lds_atomic_lock_tests)
    set_tests_properties(agc_spirv_lds_atomic_lock PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_spirv_lds_atomic_lock PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_spirv_subvector_loop_begin_tests EXCLUDE_FROM_ALL tests/SpirvSubvectorLoopBegin.cpp)
target_link_libraries(agc_spirv_subvector_loop_begin_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_subvector_loop_begin_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_subvector_loop_begin_tests)
if(BUILD_TESTING)
    set_target_properties(agc_spirv_subvector_loop_begin_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_spirv_subvector_loop_begin COMMAND agc_spirv_subvector_loop_begin_tests)
    set_tests_properties(agc_spirv_subvector_loop_begin PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_spirv_subvector_loop_begin PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_layer_input_tests EXCLUDE_FROM_ALL tests/LayerInput.cpp)
target_link_libraries(agc_layer_input_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_layer_input_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_layer_input_tests)
if(BUILD_TESTING)
    set_target_properties(agc_layer_input_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_layer_input COMMAND agc_layer_input_tests)
    set_tests_properties(agc_layer_input PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_layer_input PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
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

add_test_executable(agc_ir_reclaim_tests EXCLUDE_FROM_ALL tests/IrReclaim.cpp)
target_link_libraries(agc_ir_reclaim_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_ir_reclaim_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_ir_reclaim_tests)

add_test_executable(agc_vector_window_tests EXCLUDE_FROM_ALL tests/VectorWindow.cpp)
target_link_libraries(agc_vector_window_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vector_window_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vector_window_tests)

add_test_executable(agc_constant_bit_test_tests EXCLUDE_FROM_ALL tests/ConstantBitTest.cpp)
target_link_libraries(agc_constant_bit_test_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_constant_bit_test_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_constant_bit_test_tests)

add_test_executable(agc_integer_narrowing_tests EXCLUDE_FROM_ALL tests/IntegerNarrowing.cpp)
target_link_libraries(agc_integer_narrowing_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_integer_narrowing_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_integer_narrowing_tests)

add_test_executable(agc_scalar_select_mask_tests EXCLUDE_FROM_ALL tests/ScalarSelectMask.cpp)
target_link_libraries(agc_scalar_select_mask_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_scalar_select_mask_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_scalar_select_mask_tests)

add_test_executable(agc_tessellation_lowering_tests EXCLUDE_FROM_ALL tests/TessellationLowering.cpp)
target_link_libraries(agc_tessellation_lowering_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_tessellation_lowering_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_tessellation_lowering_tests)
if(BUILD_TESTING)
    set_target_properties(agc_tessellation_lowering_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_tessellation_lowering COMMAND agc_tessellation_lowering_tests)
    if(WIN32)
        set_tests_properties(agc_tessellation_lowering PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

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

add_test_executable(agc_redundant_value_elimination_tests EXCLUDE_FROM_ALL tests/RedundantValueElimination.cpp)
target_link_libraries(agc_redundant_value_elimination_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_redundant_value_elimination_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_redundant_value_elimination_tests)
if(BUILD_TESTING)
    set_target_properties(agc_redundant_value_elimination_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_redundant_value_elimination COMMAND agc_redundant_value_elimination_tests)
    if(WIN32)
        set_tests_properties(agc_redundant_value_elimination PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_integer_folding_tests EXCLUDE_FROM_ALL tests/IntegerFolding.cpp)
target_link_libraries(agc_integer_folding_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_integer_folding_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_integer_folding_tests)
if(BUILD_TESTING)
    set_target_properties(agc_integer_folding_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_integer_folding COMMAND agc_integer_folding_tests)
    if(WIN32)
        set_tests_properties(agc_integer_folding PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_vop1_control_decode_tests EXCLUDE_FROM_ALL tests/Vop1ControlDecode.cpp)
target_link_libraries(agc_vop1_control_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop1_control_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop1_control_decode_tests)
add_test_executable(agc_vertex_input_validation_tests EXCLUDE_FROM_ALL tests/VertexInputValidation.cpp)
target_link_libraries(agc_vertex_input_validation_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_input_validation_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_input_validation_tests)
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

add_test_executable(agc_vop3_interp_decode_tests EXCLUDE_FROM_ALL tests/Vop3InterpDecode.cpp)
target_link_libraries(agc_vop3_interp_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop3_interp_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop3_interp_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_vop3_interp_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vop3_interp_decode COMMAND agc_vop3_interp_decode_tests)
    if(WIN32)
        set_tests_properties(agc_vop3_interp_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_pipeline_queue_failure_tests EXCLUDE_FROM_ALL tests/PipelineQueueFailure.cpp
    Graphics/src/PipelineLibrary.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_pipeline_queue_failure_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pipeline_queue_failure_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_pipeline_queue_failure_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pipeline_queue_failure_tests)
if(BUILD_TESTING)
    set_target_properties(agc_pipeline_queue_failure_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(mode wait clear stop)
        add_test(NAME agc_pipeline_queue_failure_${mode} COMMAND agc_pipeline_queue_failure_tests ${mode})
        set_tests_properties(agc_pipeline_queue_failure_${mode} PROPERTIES TIMEOUT 15)
        if(WIN32)
            set_tests_properties(agc_pipeline_queue_failure_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()

add_test_executable(agc_pass_hazards_tests EXCLUDE_FROM_ALL tests/PassHazards.cpp Graphics/src/PassHazards.cpp)
target_include_directories(agc_pass_hazards_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pass_hazards_tests PRIVATE libc)
set_target_properties(agc_pass_hazards_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pass_hazards_tests)

add_test_executable(agc_barrier_batch_tests EXCLUDE_FROM_ALL tests/BarrierBatch.cpp Graphics/src/BarrierBatch.cpp)
target_include_directories(agc_barrier_batch_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_barrier_batch_tests PRIVATE libc)
set_target_properties(agc_barrier_batch_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_barrier_batch_tests)

add_test_executable(agc_dcc_key_sharing_tests EXCLUDE_FROM_ALL tests/DccKeySharing.cpp)
target_include_directories(agc_dcc_key_sharing_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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

if(BUILD_TESTING)
    add_test_executable(agc_snapshot_copy_failure_tests tests/SnapshotCopyFailure.cpp tests/AllocationFailure.cpp)
    target_include_directories(agc_snapshot_copy_failure_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    if(MINGW)
        target_compile_options(agc_snapshot_copy_failure_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    configure_windows_unwind(agc_snapshot_copy_failure_tests)
    add_test(NAME agc_snapshot_copy_failure COMMAND agc_snapshot_copy_failure_tests)
    set_tests_properties(agc_snapshot_copy_failure PROPERTIES TIMEOUT 10)
endif()
target_sources(agc_buffer_pool_tests PRIVATE Graphics/src/BufferPool.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_buffer_pool_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_buffer_pool_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_buffer_pool_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_buffer_pool_tests)
if(BUILD_TESTING)
    set_target_properties(agc_buffer_pool_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_buffer_pool COMMAND agc_buffer_pool_tests)
    add_test(NAME agc_buffer_pool_small_device_shared COMMAND agc_buffer_pool_tests --small-device-retention)
    set_tests_properties(agc_buffer_pool_small_device_shared PROPERTIES ENVIRONMENT
        "APS5_STAGING_POOL_MIB=256;APS5_BUFFER_POOL_SHARED=1")
    add_test(NAME agc_buffer_pool_small_device_no_staging COMMAND agc_buffer_pool_tests --small-device-retention)
    set_tests_properties(agc_buffer_pool_small_device_no_staging PROPERTIES ENVIRONMENT
        "APS5_STAGING_POOL_MIB=0")
    if(WIN32)
        set_tests_properties(agc_buffer_pool agc_buffer_pool_small_device_shared agc_buffer_pool_small_device_no_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_device_memory_fallback_tests EXCLUDE_FROM_ALL tests/DeviceMemoryFallback.cpp)
target_include_directories(agc_device_memory_fallback_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_device_memory_fallback_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_device_memory_fallback_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_device_memory_fallback_tests)
if(BUILD_TESTING)
    set_target_properties(agc_device_memory_fallback_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_device_memory_fallback COMMAND agc_device_memory_fallback_tests)
    if(WIN32)
        set_tests_properties(agc_device_memory_fallback PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_display_buffer_dcc_tests EXCLUDE_FROM_ALL tests/DisplayBufferDcc.cpp)
target_include_directories(agc_display_buffer_dcc_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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

add_test_executable(agc_host_hdr_tests EXCLUDE_FROM_ALL tests/HostHdr.cpp)
target_include_directories(agc_host_hdr_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_host_hdr_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_host_hdr_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_host_hdr_tests)
if(BUILD_TESTING)
    set_target_properties(agc_host_hdr_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_host_hdr COMMAND agc_host_hdr_tests)
    if(WIN32)
        set_tests_properties(agc_host_hdr PROPERTIES ENVIRONMENT_MODIFICATION
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

add_test_executable(agc_fma_mix_once_tests EXCLUDE_FROM_ALL tests/FmaMixOnce.cpp)
target_link_libraries(agc_fma_mix_once_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_fma_mix_once_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_fma_mix_once_tests)

add_test_executable(agc_f16_single_convert_tests EXCLUDE_FROM_ALL tests/F16SingleConvert.cpp)
target_link_libraries(agc_f16_single_convert_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_f16_single_convert_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_f16_single_convert_tests)

add_test_executable(agc_rect_list_position_invariant_tests EXCLUDE_FROM_ALL tests/RectListPositionInvariant.cpp)
target_link_libraries(agc_rect_list_position_invariant_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_rect_list_position_invariant_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_rect_list_position_invariant_tests)
if(BUILD_TESTING)
    set_target_properties(agc_rect_list_position_invariant_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_rect_list_position_invariant COMMAND agc_rect_list_position_invariant_tests)
    if(WIN32)
        set_tests_properties(agc_rect_list_position_invariant PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_formatted_buffer_byte_stores_tests EXCLUDE_FROM_ALL tests/FormattedBufferByteStores.cpp)
target_link_libraries(agc_formatted_buffer_byte_stores_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_formatted_buffer_byte_stores_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_formatted_buffer_byte_stores_tests)

add_test_executable(agc_mesh_output_budget_tests EXCLUDE_FROM_ALL tests/MeshOutputBudget.cpp)
target_include_directories(agc_mesh_output_budget_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_mesh_output_budget_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_mesh_output_budget_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_mesh_output_budget_tests)

add_test_executable(agc_shader_disk_cache_tests EXCLUDE_FROM_ALL tests/ShaderDiskCache.cpp)
target_include_directories(agc_shader_disk_cache_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_disk_cache_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_shader_disk_cache_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_disk_cache_tests)

add_test_executable(agc_placeholder_images_tests EXCLUDE_FROM_ALL tests/PlaceholderImages.cpp)
target_include_directories(agc_placeholder_images_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(agc_placeholder_images_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_placeholder_images_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_placeholder_images_tests)

add_test_executable(agc_screenshot_tests EXCLUDE_FROM_ALL tests/Screenshot.cpp Execution/src/Screenshot.cpp)
target_include_directories(agc_screenshot_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_screenshot_tests PRIVATE decoder_png SDL2-static libc)
set_target_properties(agc_screenshot_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_screenshot_tests)

add_test_executable(agc_shader_device_profile_tests EXCLUDE_FROM_ALL tests/ShaderDeviceProfile.cpp Execution/src/ShaderDeviceProfile.cpp)
target_include_directories(agc_shader_device_profile_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
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
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_cmask_fast_clear_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_cmask_fast_clear_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_cmask_fast_clear_tests)

set(agcBufferUpdateSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/BufferUpdate.comp)
set(agcBufferUpdateSpv ${agcTestShaderGeneratedRoot}/BufferUpdate.spv)
set(agcBufferUpdateHeader ${agcTestShaderGeneratedRoot}/BufferUpdate_spv.h)
add_custom_command(
        OUTPUT "${agcBufferUpdateHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcBufferUpdateSpv}" "${agcBufferUpdateSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcBufferUpdateSpv} -DOUTPUT=${agcBufferUpdateHeader}
        -DSYMBOL=BUFFER_UPDATE_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcBufferUpdateSource}" glslang-standalone
        VERBATIM
)

add_test_executable(agc_driver_recorder_tests EXCLUDE_FROM_ALL
        tests/Recorder.cpp
        tests/HostImportLifetime.cpp
        tests/ResidentPresent.cpp
        ${agcTriangleHeaders}
        ${agcSampleLodShaderHeader}
        ${agcSampleArrayShaderHeader}
        ${agcCoverShaderHeaders}
        ${agcBufferUpdateHeader}
)

target_include_directories(agc_driver_recorder_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_recorder_tests PRIVATE
        libSceAgc
        libkernel
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_recorder_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_recorder_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_recorder_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_recorder_device_selection COMMAND agc_driver_recorder_tests --device-selection-only)
    add_test(NAME agc_gpu_timing_complete COMMAND agc_driver_recorder_tests --gpu-timing-complete-only)
    add_test(NAME agc_gpu_timing_incomplete COMMAND agc_driver_recorder_tests --gpu-timing-incomplete-only)
    set_tests_properties(agc_gpu_timing_complete agc_gpu_timing_incomplete PROPERTIES
        ENVIRONMENT "APS5_PROFILE_GPU=1" TIMEOUT 15 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_recorder_device_selection agc_gpu_timing_complete agc_gpu_timing_incomplete PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_command_memory_reuse COMMAND agc_driver_recorder_tests --command-reuse-only)
    set_tests_properties(agc_command_memory_reuse PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_command_memory_reuse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pipeline_cache COMMAND agc_driver_recorder_tests --pipeline-cache-only)
    add_test(NAME agc_pipeline_cache_file COMMAND agc_driver_recorder_tests --pipeline-cache-file-only)
    add_test(NAME agc_pipeline_cache_header COMMAND agc_driver_recorder_tests --pipeline-cache-header-only)
    set_tests_properties(agc_pipeline_cache_header PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_cache_header PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pipeline_library COMMAND agc_driver_recorder_tests --pipeline-library-only)
    add_test(NAME agc_pipeline_library_off COMMAND agc_driver_recorder_tests --pipeline-library-only)
    set_tests_properties(agc_pipeline_library agc_pipeline_library_off PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(agc_pipeline_library_off PROPERTIES ENVIRONMENT "APS5_NO_GPL=1")
    add_test(NAME agc_pipeline_library_fast_only COMMAND agc_driver_recorder_tests --pipeline-library-only)
    set_tests_properties(agc_pipeline_library_fast_only PROPERTIES ENVIRONMENT "APS5_NO_PIPELINE_LTO=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_library agc_pipeline_library_off agc_pipeline_library_fast_only PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_pipeline_cache agc_pipeline_cache_file PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_cache agc_pipeline_cache_file PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_import_watch COMMAND agc_driver_recorder_tests --import-watch-only)
    set_tests_properties(agc_import_watch PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        add_test(NAME agc_import_watch_other_family COMMAND agc_driver_recorder_tests --import-watch-other-family-only)
        set_tests_properties(agc_import_watch_other_family PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME agc_import_settle_deferred COMMAND agc_driver_recorder_tests --deferred-settle-only)
        add_test(NAME agc_import_settle_deferred_other_family COMMAND agc_driver_recorder_tests --deferred-settle-other-family-only)
        add_test(NAME agc_import_settle_write_order COMMAND agc_driver_recorder_tests --settle-write-order-only)
        add_test(NAME agc_import_settle_write_order_other_family COMMAND agc_driver_recorder_tests --settle-write-order-other-family-only)
        add_test(NAME agc_import_settle_wait_failure COMMAND agc_driver_recorder_tests --settle-wait-failure-only)
        set_tests_properties(agc_import_settle_write_order agc_import_settle_write_order_other_family agc_import_settle_wait_failure PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 30
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_import_settle_first_use COMMAND agc_driver_recorder_tests --settle-first-use-only)
        add_test(NAME agc_import_settle_earlier_import COMMAND agc_driver_recorder_tests --settle-earlier-import-only)
        add_test(NAME agc_import_settle_retire COMMAND agc_driver_recorder_tests --settle-retire-only)
        add_test(NAME agc_import_settle_storage_image COMMAND agc_driver_recorder_tests --settle-storage-image-only)
        set_tests_properties(agc_import_settle_deferred agc_import_settle_deferred_other_family agc_import_settle_first_use agc_import_settle_earlier_import agc_import_settle_retire agc_import_settle_storage_image PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
        set_tests_properties(agc_import_watch agc_import_watch_other_family agc_import_settle_deferred agc_import_settle_deferred_other_family agc_import_settle_first_use agc_import_settle_earlier_import agc_import_settle_retire agc_import_settle_storage_image PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        add_test(NAME agc_reimport_bench COMMAND agc_driver_recorder_tests --reimport-bench)
        add_test(NAME agc_host_import_cut_lifetime COMMAND agc_driver_recorder_tests --host-import-cut-only)
        set_tests_properties(agc_reimport_bench agc_host_import_cut_lifetime PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(agc_reimport_bench agc_host_import_cut_lifetime PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_unchanged_cpu_stamps COMMAND agc_driver_recorder_tests --unchanged-cpu-stamps-only)
    set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(mode IN ITEMS map ring shared uncached)
        add_test(NAME agc_collect_mapping_history_${mode} COMMAND agc_driver_recorder_tests --collect-mapping-history-only)
        set_tests_properties(agc_collect_mapping_history_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_collect_mapping_history_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_collect_mapping_history_ring PROPERTIES ENVIRONMENT "APS5_NO_EPOCH_COLLECT_MAP=1")
    set_tests_properties(agc_collect_mapping_history_shared PROPERTIES ENVIRONMENT "APS5_SHARED_COLLECT_MEMO=1")
    set_tests_properties(agc_collect_mapping_history_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
    if(WIN32)
        foreach(mode IN ITEMS cached uncached)
            add_test(NAME agc_hint_mapping_history_${mode} COMMAND agc_driver_recorder_tests --hint-mapping-history-only)
            set_tests_properties(agc_hint_mapping_history_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endforeach()
        set_tests_properties(agc_hint_mapping_history_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
    endif()
    if(WIN32)
        add_test(NAME agc_partial_watch_restoration COMMAND agc_driver_recorder_tests --partial-watch-restoration-only)
        set_tests_properties(agc_partial_watch_restoration PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_kept_unit_settle COMMAND agc_driver_recorder_tests --kept-unit-settle-only)
    set_tests_properties(agc_storage_kept_unit_settle PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_kept_unit_settle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_baseline_settled COMMAND agc_driver_recorder_tests --settled-baseline-only)
    add_test(NAME agc_storage_clear_hand_over COMMAND agc_driver_recorder_tests --clear-hand-over-only)
    set_tests_properties(agc_storage_clear_hand_over PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_clear_hand_over PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_integer_clear COMMAND agc_driver_recorder_tests --integer-clear-only)
    set_tests_properties(agc_storage_integer_clear PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_integer_clear PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_upload_lifetime COMMAND agc_driver_recorder_tests --storage-upload-lifetime-only)
    add_test(NAME agc_sampled_storage_base_level_usage COMMAND agc_driver_recorder_tests --sampled-storage-base-level-usage-only)
    set_tests_properties(agc_sampled_storage_base_level_usage PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_sampled_storage_base_level_usage PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_upload_failure COMMAND agc_driver_recorder_tests --storage-upload-failure-only)
    set_tests_properties(agc_storage_upload_lifetime agc_storage_upload_failure PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_upload_lifetime agc_storage_upload_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_storage_baseline_settled PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_baseline_settled PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_bda_staging COMMAND agc_driver_recorder_tests --bda-staging-only)
    set_tests_properties(agc_bda_staging PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_bda_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_read_written_staging COMMAND agc_driver_recorder_tests --read-written-staging-only)
    set_tests_properties(agc_read_written_staging PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_read_written_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_written_buffer_order COMMAND agc_driver_recorder_tests --written-buffer-order-only)
    set_tests_properties(agc_storage_written_buffer_order PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_written_buffer_order PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_split_writeback COMMAND agc_driver_recorder_tests --split-writeback-only)
    set_tests_properties(agc_storage_split_writeback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_split_writeback PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_write_back COMMAND agc_driver_recorder_tests --storage-write-back-only)
    set_tests_properties(agc_storage_write_back PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_write_back PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_copied_direct_ordering COMMAND agc_driver_recorder_tests --copied-direct-only)
    set_tests_properties(agc_copied_direct_ordering PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_copied_direct_ordering PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_writeback_blocks COMMAND agc_driver_recorder_tests --writeback-blocks-only)
    set_tests_properties(agc_storage_writeback_blocks PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_writeback_blocks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_recorder_read_list COMMAND agc_driver_recorder_tests --read-list-only)
    set_tests_properties(agc_recorder_read_list PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_deferred_failure_report COMMAND agc_driver_recorder_tests --deferred-failure-report-only)
    set_tests_properties(agc_recorder_deferred_failure_report PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_recorder_deferred_failure_report PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_space_pending_writes COMMAND agc_driver_recorder_tests --space-pending-writes-only)
    set_tests_properties(agc_space_pending_writes PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_space_pending_writes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_host_import_unmap COMMAND agc_driver_recorder_tests --host-import-unmap-only)
    set_tests_properties(agc_host_import_unmap PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_parked_import_unmap COMMAND agc_driver_recorder_tests --parked-import-unmap-only)
    set_tests_properties(agc_parked_import_unmap PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    add_test(NAME agc_host_import_retirement COMMAND agc_driver_recorder_tests --host-import-retirement-only)
    set_tests_properties(agc_host_import_retirement PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sparse_run_import COMMAND agc_driver_recorder_tests --sparse-run-import-only)
    set_tests_properties(agc_sparse_run_import PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sparse_run_import_copied COMMAND agc_driver_recorder_tests --sparse-run-copied-only)
    set_tests_properties(agc_sparse_run_import_copied PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_unit_shadow COMMAND agc_driver_recorder_tests --unit-shadow-only)
    set_tests_properties(agc_unit_shadow PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    add_test(NAME agc_idle_publish_wait COMMAND agc_driver_recorder_tests --idle-publish-wait-only)
    set_tests_properties(agc_idle_publish_wait PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000;APS5_WRITE_WATCH_IMPORTS=watch")
    add_test(NAME agc_driver_bda_reads_shadowed_results COMMAND agc_driver_recorder_tests --bda-shadowed-results-only)
    add_test(NAME agc_driver_bda_reads_shadowed_results_per_build COMMAND agc_driver_recorder_tests --bda-shadowed-results-only)
    set_tests_properties(agc_driver_bda_reads_shadowed_results agc_driver_bda_reads_shadowed_results_per_build PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_driver_bda_reads_shadowed_results_per_build PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_SPACE_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_driver_bda_reads_shadowed_results agc_driver_bda_reads_shadowed_results_per_build PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_unit_shadow_completion_store COMMAND agc_driver_recorder_tests --completion-store-shadow-only)
    set_tests_properties(agc_unit_shadow_completion_store PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_unit_shadow_completion_store PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        add_test(NAME agc_storage_cpu_snapshot COMMAND agc_driver_recorder_tests --storage-cpu-snapshot-only)
        set_tests_properties(agc_storage_cpu_snapshot PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_descriptor_pool_recycling COMMAND agc_driver_recorder_tests --descriptor-pool-only)
    set_tests_properties(agc_descriptor_pool_recycling PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_snapshot_cap COMMAND agc_driver_recorder_tests --draw-snapshot-cap-only)
    set_tests_properties(agc_draw_snapshot_cap PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_snapshot_epoch COMMAND agc_driver_recorder_tests --draw-snapshot-epoch-only)
    set_tests_properties(agc_draw_snapshot_epoch PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_multisample_guest_access COMMAND agc_driver_recorder_tests --multisample-only)
    set_tests_properties(agc_multisample_guest_access PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_inputs_in_place COMMAND agc_driver_recorder_tests --draw-inputs-in-place-only)
    add_test(NAME agc_pending_write_snapshot COMMAND agc_driver_recorder_tests --pending-write-snapshot-only)
    set_tests_properties(agc_pending_write_snapshot PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_pending_write_snapshot PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_draw_inputs_in_place PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_inputs_copied COMMAND agc_driver_recorder_tests --draw-inputs-copied-only)
    set_tests_properties(agc_draw_inputs_copied PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_resident_present COMMAND agc_driver_recorder_tests --resident-present-only)
    set_tests_properties(agc_resident_present PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_queued_write_groups COMMAND agc_driver_recorder_tests --queued-write-groups-only)
    add_test(NAME agc_tiled_resolve COMMAND agc_driver_recorder_tests --tiled-resolve-only)
    set_tests_properties(agc_tiled_resolve PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_slice_dirty COMMAND agc_driver_recorder_tests --slice-dirty-only)
    set_tests_properties(agc_slice_dirty PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_metadata_pass COMMAND agc_driver_recorder_tests --metadata-pass-only)
    set_tests_properties(agc_metadata_pass PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_recorder_read_list agc_host_import_unmap agc_parked_import_unmap agc_host_import_retirement agc_sparse_run_import agc_sparse_run_import_copied agc_unit_shadow agc_idle_publish_wait agc_descriptor_pool_recycling agc_draw_snapshot_cap agc_draw_snapshot_epoch agc_multisample_guest_access agc_draw_inputs_in_place agc_draw_inputs_copied agc_resident_present agc_slice_dirty agc_queued_write_groups agc_metadata_pass agc_tiled_resolve PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_host_import_second_mapping COMMAND agc_driver_recorder_tests --host-import-second-mapping-only)
        set_tests_properties(agc_host_import_second_mapping PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pending_key_stores COMMAND agc_driver_recorder_tests --pending-key-stores-only)
    set_tests_properties(agc_pending_key_stores PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_shadowed_key_stores COMMAND agc_driver_recorder_tests --shadowed-key-stores-only)
    set_tests_properties(agc_shadowed_key_stores PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sample_dumps COMMAND agc_driver_recorder_tests --sample-dumps-only)
    set_tests_properties(agc_sample_dumps PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_uncounted_passes COMMAND agc_driver_recorder_tests --uncounted-passes-only)
    set_tests_properties(agc_uncounted_passes PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_draw_descriptors COMMAND agc_driver_recorder_tests --draw-descriptors-only)
    set_tests_properties(agc_draw_descriptors PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_pending_key_stores agc_shadowed_key_stores agc_sample_dumps agc_uncounted_passes agc_draw_descriptors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_host_import_lifetime COMMAND agc_driver_recorder_tests --host-import-lifetime-only)
    add_test(NAME agc_driver_host_import_mutation_race COMMAND agc_driver_recorder_tests --host-import-mutation-race-only)
    set_tests_properties(agc_driver_host_import_lifetime agc_driver_host_import_mutation_race PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_driver_unit_shadow COMMAND agc_driver_recorder_tests --unit-shadow-only)
    add_test(NAME agc_driver_unit_shadow_budget COMMAND agc_driver_recorder_tests --unit-shadow-only)
    set_tests_properties(agc_driver_unit_shadow PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_driver_unit_shadow_budget PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000;APS5_UNIT_SHADOW_MIB=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_completion_labels COMMAND agc_driver_recorder_tests --completion-labels-only)
    set_tests_properties(agc_recorder_completion_labels PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_cmask COMMAND agc_driver_recorder_tests --cmask-only)
    set_tests_properties(agc_recorder_cmask PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_cube COMMAND agc_driver_recorder_tests --cube-only)
    set_tests_properties(agc_recorder_cube PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_host_import_lifetime agc_driver_host_import_mutation_race agc_driver_unit_shadow agc_driver_unit_shadow_budget PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_recorder_completion_labels agc_recorder_cmask agc_recorder_cube PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_property(TEST agc_driver_host_import_lifetime APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "APS5_RELEASE_UNDER_LOCK=unset:" "APS5_RELEASE_ON_UNLOCK=unset:")
    if(WIN32)
        add_test(NAME agc_driver_host_import_lifetime_other_family COMMAND agc_driver_recorder_tests --host-import-lifetime-other-family-only)
        set_tests_properties(agc_driver_host_import_lifetime_other_family PROPERTIES
            ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_property(TEST agc_driver_host_import_lifetime_other_family APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_RELEASE_UNDER_LOCK=unset:" "APS5_RELEASE_ON_UNLOCK=unset:")
    endif()
endif()

add_test_executable(agc_recorder_kept_bytes_tests EXCLUDE_FROM_ALL tests/RecorderKeptBytes.cpp)
target_include_directories(agc_recorder_kept_bytes_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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

add_test_executable(agc_command_batch_failure_tests EXCLUDE_FROM_ALL
        tests/CommandBatch.cpp tests/CommandBatchFailure.cpp Graphics/src/Resources.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_command_batch_failure_tests PRIVATE
        ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_command_batch_failure_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_command_batch_failure_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_command_batch_failure_tests)
if(BUILD_TESTING)
    set_target_properties(agc_command_batch_failure_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_command_batch_failure COMMAND agc_command_batch_failure_tests)
    set_tests_properties(agc_command_batch_failure PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_command_batch_failure PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_host_import_retire_tests EXCLUDE_FROM_ALL tests/HostImportRetireHold.cpp)
target_include_directories(agc_host_import_retire_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_host_import_retire_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_host_import_retire_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_host_import_retire_tests)
target_sources(agc_host_import_retire_tests PRIVATE tests/HostImportFailure.cpp)
if(BUILD_TESTING)
    set_target_properties(agc_host_import_retire_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_host_import_retire COMMAND agc_host_import_retire_tests)
    if(WIN32)
        set_tests_properties(agc_host_import_retire PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_host_import_sparse_tests EXCLUDE_FROM_ALL tests/HostImportSparseDmaBuf.cpp)
target_include_directories(agc_host_import_sparse_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_host_import_sparse_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_host_import_sparse_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_host_import_sparse_tests)
if(BUILD_TESTING)
    set_target_properties(agc_host_import_sparse_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_host_import_sparse COMMAND agc_host_import_sparse_tests)
    set_tests_properties(agc_host_import_sparse PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_host_import_sparse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_driver_presentation_pass_tests EXCLUDE_FROM_ALL
        tests/PresentationPass.cpp
)

target_include_directories(agc_driver_presentation_pass_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_presentation_pass_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_presentation_pass_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_presentation_pass_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_presentation_pass_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_presentation_pass COMMAND agc_driver_presentation_pass_tests)
    set_tests_properties(agc_driver_presentation_pass PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_presentation_pass PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_performance_overlay_statistics_tests EXCLUDE_FROM_ALL
        tests/PerformanceOverlayStatistics.cpp
)
target_include_directories(agc_performance_overlay_statistics_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_performance_overlay_statistics_tests PRIVATE
        libSceAgcDriver
        libc
)
set_target_properties(agc_performance_overlay_statistics_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_performance_overlay_statistics_tests)

add_test_executable(agc_performance_overlay_tests EXCLUDE_FROM_ALL
        tests/PerformanceOverlay.cpp
)
target_include_directories(agc_performance_overlay_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_performance_overlay_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)
set_target_properties(agc_performance_overlay_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_performance_overlay_tests)

if(BUILD_TESTING)
    set_target_properties(agc_performance_overlay_statistics_tests agc_performance_overlay_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_performance_overlay_statistics COMMAND agc_performance_overlay_statistics_tests)
    add_test(NAME agc_performance_overlay COMMAND agc_performance_overlay_tests)
    add_test(NAME agc_performance_overlay_device_selection COMMAND agc_performance_overlay_tests device-selection)
    add_test(NAME agc_performance_overlay_device_selection_cpu COMMAND agc_performance_overlay_tests device-selection-cpu)
    set_tests_properties(agc_performance_overlay agc_performance_overlay_device_selection PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_performance_overlay_device_selection_cpu PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_performance_overlay_statistics agc_performance_overlay agc_performance_overlay_device_selection agc_performance_overlay_device_selection_cpu PROPERTIES ENVIRONMENT_MODIFICATION
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
        shader_recompiler_agc_driver
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
        tests/LastRecordIndices.cpp
)

if(BUILD_TESTING)
    add_test_executable(agc_descriptor_cache_failure_tests
        tests/DescriptorCacheFailure.cpp
        Graphics/src/ShaderResources.cpp)
    target_include_directories(agc_descriptor_cache_failure_tests PRIVATE
        $<TARGET_PROPERTY:libSceAgcDriver,INCLUDE_DIRECTORIES>)
    target_link_libraries(agc_descriptor_cache_failure_tests PRIVATE libSceAgcDriver shader_recompiler_agc_driver libc libkernel)
    configure_windows_unwind(agc_descriptor_cache_failure_tests)
    add_test(NAME agc_descriptor_cache_failure COMMAND agc_descriptor_cache_failure_tests)
    set_tests_properties(agc_descriptor_cache_failure PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_descriptor_cache_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

target_include_directories(agc_driver_graphics_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_graphics_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)

if(BUILD_TESTING)
    add_test(NAME agc_command_batch_fence_failure COMMAND agc_driver_graphics_tests --command-batch-fence-failure)
    set_tests_properties(agc_command_batch_fence_failure PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_command_batch_fence_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

set_target_properties(agc_driver_graphics_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_graphics_tests)

add_test_executable(agc_driver_msaa_layout_tests EXCLUDE_FROM_ALL
        tests/MultisampleTextureLayout.cpp
        Graphics/src/MultisampleTextureLayout.cpp
)
target_include_directories(agc_driver_msaa_layout_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_msaa_layout_tests PRIVATE libc)
set_target_properties(agc_driver_msaa_layout_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_msaa_layout_tests)
if(BUILD_TESTING)
    set_target_properties(agc_driver_msaa_layout_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_msaa_layout COMMAND agc_driver_msaa_layout_tests)
    set_tests_properties(agc_driver_msaa_layout PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_driver_msaa_layout PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

foreach(agcVertexRangeShader IN ITEMS VertexRange.vert VertexRangeProducer.frag)
    string(REPLACE "." "_" agcVertexRangeName "${agcVertexRangeShader}")
    string(TOUPPER "${agcVertexRangeName}_SPV" agcVertexRangeSymbol)
    set(agcVertexRangeSpv ${agcTestShaderGeneratedRoot}/${agcVertexRangeName}.spv)
    set(agcVertexRangeHeader ${agcTestShaderGeneratedRoot}/${agcVertexRangeName}_spv.h)
    add_custom_command(
            OUTPUT "${agcVertexRangeHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcVertexRangeSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/${agcVertexRangeShader}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcVertexRangeSpv} -DOUTPUT=${agcVertexRangeHeader}
            -DSYMBOL=${agcVertexRangeSymbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/${agcVertexRangeShader}" glslang-standalone
            VERBATIM
    )
    list(APPEND agcVertexRangeHeaders ${agcVertexRangeHeader})
endforeach()

target_sources(agc_driver_recorder_tests PRIVATE ${agcVertexRangeHeaders})

if(BUILD_TESTING)
    add_test(NAME agc_driver_vertex_target_range_copied COMMAND agc_driver_recorder_tests --vertex-target-range-copied)
    add_test(NAME agc_driver_vertex_target_range_imported COMMAND agc_driver_recorder_tests --vertex-target-range-imported)
    set_tests_properties(agc_driver_vertex_target_range_copied agc_driver_vertex_target_range_imported PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_vertex_target_range_copied agc_driver_vertex_target_range_imported PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_driver_bda_device_tests EXCLUDE_FROM_ALL
        tests/BdaDevice.cpp
        tests/BdaShader.cpp
        tests/BdaExecution.cpp
        tests/BdaContracts.cpp
        tests/HostImportFailure.cpp
)

target_include_directories(agc_driver_bda_device_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_bda_device_tests PRIVATE libSceAgcDriver SDL2-static libc)
set_target_properties(agc_driver_bda_device_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_bda_device_tests)
if(BUILD_TESTING)
    add_test(NAME agc_driver_host_import_failure_handles COMMAND agc_driver_bda_device_tests --host-import-failures)
    set_tests_properties(agc_driver_host_import_failure_handles PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_host_import_failure_handles PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_driver_mesh_tests EXCLUDE_FROM_ALL tests/Mesh.cpp)
target_include_directories(agc_driver_mesh_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_mesh_tests PRIVATE libSceAgcDriver SDL2-static libc)
set_target_properties(agc_driver_mesh_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_mesh_tests)

set(ANYPS5_AGC_EXECUTION_TESTS "" CACHE STRING "tests/execution sources to build, by file name without extension; the others build a stub that reports a skip. Empty builds all")
file(GLOB agcExecutionTestSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/*.cpp)
set(agcExecutionTests)
foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)
    get_filename_component(agcExecutionTestName ${agcExecutionTestSource} NAME_WE)
    if(ANYPS5_AGC_EXECUTION_TESTS AND NOT agcExecutionTestName IN_LIST ANYPS5_AGC_EXECUTION_TESTS)
        set(agcExecutionTestSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/ExecutionTestNotBuilt.cpp)
    endif()
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" agcExecutionTestName "${agcExecutionTestName}")
    string(TOLOWER "agc_driver_${agcExecutionTestName}_tests" agcExecutionTest)
    add_test_executable(${agcExecutionTest} EXCLUDE_FROM_ALL ${agcExecutionTestSource})
    target_include_directories(${agcExecutionTest} PRIVATE
            ${LIBS_INCLUDE_DIR}
            $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )
    target_link_libraries(${agcExecutionTest} PRIVATE libSceAgcDriver SDL2-static libc)
    set_target_properties(${agcExecutionTest} PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(${agcExecutionTest})
    list(APPEND agcExecutionTests ${agcExecutionTest})
endforeach()
if(BUILD_TESTING)
    add_test(NAME agc_driver_address_passes_targets_split COMMAND agc_driver_address_passes_tests --targets)
    set_tests_properties(agc_driver_address_passes_targets_split PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_READ_PASSES=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_address_passes_targets_split PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_target_properties(agc_driver_rcp_hardware_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_rcp_hardware_exact COMMAND agc_driver_rcp_hardware_tests)
    set_tests_properties(agc_driver_rcp_hardware_exact PROPERTIES ENVIRONMENT "APS5_EXACT_RCP=1;ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_rcp_hardware_exact PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_sampling_evicting COMMAND agc_driver_depth_plane_sampling_tests --evicting)
    set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT "APS5_DEPTH_PLANE_COPY_MIB=1" SKIP_RETURN_CODE 77 TIMEOUT 120)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
foreach(agcExecutionTestName IN LISTS ANYPS5_AGC_EXECUTION_TESTS)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/${agcExecutionTestName}.cpp)
        message(FATAL_ERROR "ANYPS5_AGC_EXECUTION_TESTS names ${agcExecutionTestName}, which has no source in tests/execution")
    endif()
endforeach()
if(BUILD_TESTING)
    add_test(NAME agc_driver_indirect_thread_dimensions_cpu COMMAND agc_driver_indirect_thread_dimensions_tests)
    set_tests_properties(agc_driver_indirect_thread_dimensions_cpu PROPERTIES ENVIRONMENT "APS5_NO_GPU_THREAD_DIMENSIONS=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_indirect_thread_dimensions_cpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_compile_definitions(agc_driver_stencil_null_feedback_tests PRIVATE ANYPS5_ENABLE_SPIRV_TOOLS=1)
    target_link_libraries(agc_driver_stencil_null_feedback_tests PRIVATE SPIRV-Tools-static)
endif()

if(NOT ANYPS5_AGC_EXECUTION_TESTS OR "PooledBufferRange" IN_LIST ANYPS5_AGC_EXECUTION_TESTS)
    target_sources(agc_driver_pooled_buffer_range_tests PRIVATE
            Graphics/src/BufferPool.cpp Graphics/src/Resources.cpp tests/AllocationFailure.cpp tests/CommandBatchFailure.cpp)
endif()

target_link_libraries(agc_driver_gpu_map_on_fiber_tests PRIVATE libSceFiber)

target_link_libraries(agc_driver_rect_list_corner_tests PRIVATE shader_recompiler_agc_driver)
target_link_libraries(agc_driver_queue_zero_late_interrupt_tests PRIVATE libkernel)

target_link_libraries(agc_driver_storage_file_write_tests PRIVATE libkernel)

target_link_libraries(agc_driver_preparation_scope_settle_tests PRIVATE libSceAgc)

target_include_directories(agc_driver_alpha_to_coverage_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_sample_multisample_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_image_table_cache_bounds_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_draw_cache_relocation_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

if(BUILD_TESTING)
    add_test(NAME agc_submit_progress COMMAND agc_driver_submit_progress_tests)
    set_tests_properties(agc_submit_progress PROPERTIES
            ENVIRONMENT "APS5_SUBMIT_PROGRESS=1;ANYPS5_REQUIRE_VULKAN=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_submit_progress PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_submit_progress_no_timeline COMMAND agc_driver_submit_progress_tests --fallback-only)
    set_tests_properties(agc_submit_progress_no_timeline PROPERTIES
            ENVIRONMENT "APS5_SUBMIT_PROGRESS=1;APS5_NO_TIMELINE=1;ANYPS5_REQUIRE_VULKAN=1"
            TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_submit_progress_no_timeline PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME agc_driver_store_pending_identity COMMAND agc_driver_store_over_untracked_tests --pending-identity)
    set_tests_properties(agc_driver_store_pending_identity PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_store_idle_results_wake COMMAND agc_driver_store_idle_results_tests --idle-epoch-wake)
    set_tests_properties(agc_driver_store_idle_results_wake PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_store_pending_identity PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_store_idle_results_wake PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME agc_driver_native_fmask COMMAND agc_driver_sample_multisample_target_tests --native-fmask)
    set_tests_properties(agc_driver_native_fmask PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_native_fmask PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_native_msaa_vertex_input COMMAND agc_driver_sample_multisample_target_tests --native-vertex-input)
    set_tests_properties(agc_driver_native_msaa_vertex_input PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_native_msaa_vertex_input PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_undithered_alpha_to_coverage_state COMMAND agc_driver_graphics_tests --undithered-alpha-to-coverage-state)
    add_test(NAME agc_driver_depth_bounds_state COMMAND agc_driver_graphics_tests --depth-bounds-state)
    add_test(NAME agc_driver_msaa_dcc_state COMMAND agc_driver_graphics_tests --multisample-dcc-state)
    add_test(NAME agc_driver_unused_viewport_depth COMMAND agc_driver_graphics_tests --unused-viewport-depth)
    set_tests_properties(agc_driver_unused_viewport_depth PROPERTIES TIMEOUT 30)
    set_tests_properties(agc_driver_undithered_alpha_to_coverage_state agc_driver_depth_bounds_state PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_alpha_to_coverage_state COMMAND agc_driver_graphics_tests --alpha-to-coverage-state)
    set_tests_properties(agc_driver_alpha_to_coverage_state PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_dithered_alpha_to_coverage COMMAND agc_driver_alpha_to_coverage_tests --dithered8)
    set(multisampledDccClearTests)
    foreach(samples IN ITEMS 2 4 8)
        set(testName "agc_driver_msaa_dcc_clear_${samples}")
        add_test(NAME ${testName} COMMAND agc_driver_alpha_to_coverage_tests --dcc-fast-clear ${samples})
        list(APPEND multisampledDccClearTests ${testName})
    endforeach()
    set_tests_properties(agc_driver_msaa_dcc_state ${multisampledDccClearTests} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(${multisampledDccClearTests} PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(agc_driver_dithered_alpha_to_coverage PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_alpha_to_coverage_state agc_driver_undithered_alpha_to_coverage_state agc_driver_depth_bounds_state agc_driver_msaa_dcc_state agc_driver_unused_viewport_depth PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_dithered_alpha_to_coverage ${multisampledDccClearTests} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
target_include_directories(agc_driver_native_multisample_depth_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

foreach(agcReadbackTestKind IN ITEMS Vertex Uint Depth)
    string(TOUPPER ${agcReadbackTestKind} agcReadbackTestSymbol)
    if(agcReadbackTestKind STREQUAL "Vertex")
        set(agcReadbackTestStage vert)
    else()
        set(agcReadbackTestStage frag)
    endif()
    if(agcReadbackTestKind STREQUAL "Depth")
        set(agcReadbackTestDepth 1)
    else()
        set(agcReadbackTestDepth 0)
    endif()
    set(agcReadbackTestHeader ${agcTestShaderGeneratedRoot}/MsaaReadback${agcReadbackTestKind}_spv.h)
    set(agcReadbackTestSpv ${agcTestShaderGeneratedRoot}/MsaaReadback${agcReadbackTestKind}.spv)
    add_custom_command(
            OUTPUT "${agcReadbackTestHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -DREAD_DEPTH=${agcReadbackTestDepth} -o "${agcReadbackTestSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/MultisampleReadback.${agcReadbackTestStage}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcReadbackTestSpv} -DOUTPUT=${agcReadbackTestHeader}
            -DSYMBOL=MSAA_READBACK_${agcReadbackTestSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/MultisampleReadback.${agcReadbackTestStage}" glslang-standalone
            VERBATIM
    )
    target_sources(agc_driver_multisample_readback_tests PRIVATE ${agcReadbackTestHeader})
endforeach()
target_include_directories(agc_driver_multisample_readback_tests PRIVATE ${agcTestShaderGeneratedRoot})

target_include_directories(agc_driver_dual_source_blend_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_reversed_order_blend_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_swap_alt_blend_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_packed_float_color_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(agc_driver_job_manager_compute_tests PRIVATE libSceJobManager libSceAgc libkernel)

target_include_directories(agc_driver_packed_unorm_color_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_packed_unorm4444_color_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_packed_unorm4444_partial_write_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_packed_unorm4444_multisample_partial_write_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_color1555_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_color565_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_depth_range_remap_draw_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

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

foreach(agcSampleOrderTestVariant IN ITEMS "R8;SampleOrderTexels.comp;-DTEXEL_FORMAT=r8ui" "R16;SampleOrderTexels.comp;-DTEXEL_FORMAT=r16ui" "R32;SampleOrderTexels.comp;-DTEXEL_FORMAT=r32ui" "RG32;SampleOrderTexels.comp;-DTEXEL_FORMAT=rg32ui" "RGBA32;SampleOrderTexels.comp;-DTEXEL_FORMAT=rgba32ui" "FillVertex;SampleOrderFill.vert;-DPLAIN=1" "FillDepth;SampleOrderFill.frag;-DPLAIN=1" "FillStencil;SampleOrderFill.frag;-DSTENCIL=1" "ReadDepth;SampleOrderRead.comp;-DPLAIN=1" "ReadStencil;SampleOrderRead.comp;-DSTENCIL=1" "ReadColor;SampleOrderRead.comp;-DCOLOR=1")
    list(GET agcSampleOrderTestVariant 0 agcSampleOrderTestName)
    list(GET agcSampleOrderTestVariant 1 agcSampleOrderTestFile)
    list(GET agcSampleOrderTestVariant 2 agcSampleOrderTestDefine)
    string(TOUPPER "${agcSampleOrderTestName}" agcSampleOrderTestSymbol)
    set(agcSampleOrderTestSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/${agcSampleOrderTestFile})
    set(agcSampleOrderTestSpv ${agcTestShaderGeneratedRoot}/SampleOrderTest${agcSampleOrderTestName}.spv)
    set(agcSampleOrderTestHeader ${agcTestShaderGeneratedRoot}/SampleOrderTest${agcSampleOrderTestName}_spv.h)
    add_custom_command(
        OUTPUT "${agcSampleOrderTestHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 ${agcSampleOrderTestDefine} -o "${agcSampleOrderTestSpv}" "${agcSampleOrderTestSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleOrderTestSpv} -DOUTPUT=${agcSampleOrderTestHeader}
        -DSYMBOL=SAMPLE_ORDER_TEST_${agcSampleOrderTestSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleOrderTestSource}" glslang-standalone
        VERBATIM
    )
    target_sources(agc_driver_sample_order_tests PRIVATE ${agcSampleOrderTestHeader})
endforeach()
target_include_directories(agc_driver_sample_order_tests PRIVATE ${agcTestShaderGeneratedRoot})

target_include_directories(agc_driver_prepared_exports_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_pipeline_journal_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_buffer_descriptor_runtime_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_shutdown_threads_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_image_converted_unorm_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(agc_driver_failed_capture_dump_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_publication_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(agc_driver_prepared_graphics_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(agc_driver_interpolation_modes_tests PRIVATE shader_recompiler_agc_driver)

target_include_directories(agc_driver_prepared_unregistered_graphics_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(agc_driver_guest_memory_lifetime_tests PRIVATE libkernel)

target_include_directories(agc_driver_prepared_shaders_tests PRIVATE
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
)

if(BUILD_TESTING)
    set_target_properties(agc_shader_mode_register_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(packedStorageFormat IN ITEMS 133 134 136)
        set(packedStorageTest agc_driver_image_packed16_storage_${packedStorageFormat})
        add_test(NAME ${packedStorageTest} COMMAND agc_driver_image_converted_unorm_tests --packed16 ${packedStorageFormat})
        set_tests_properties(${packedStorageTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        if(WIN32)
            set_tests_properties(${packedStorageTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    foreach(prebuildPolicyMode IN ITEMS unset 0 1)
        set(prebuildPolicyTest agc_driver_prepared_graphics_prebuild_${prebuildPolicyMode})
        add_test(NAME ${prebuildPolicyTest} COMMAND agc_driver_prepared_graphics_tests --prebuild-${prebuildPolicyMode})
        set_tests_properties(${prebuildPolicyTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        set(prebuildPolicyEnvironment "APS5_NO_GPL=unset:")
        if(prebuildPolicyMode STREQUAL "unset")
            list(APPEND prebuildPolicyEnvironment "APS5_NO_LIBRARY_PREBUILD=unset:")
        else()
            list(APPEND prebuildPolicyEnvironment "APS5_NO_LIBRARY_PREBUILD=set:${prebuildPolicyMode}")
        endif()
        if(WIN32)
            list(APPEND prebuildPolicyEnvironment "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        set_tests_properties(${prebuildPolicyTest} PROPERTIES ENVIRONMENT_MODIFICATION "${prebuildPolicyEnvironment}")
    endforeach()
    add_test(NAME agc_driver_readonly_resummarize COMMAND agc_driver_tests --readonly-resummarize)
    set_tests_properties(agc_driver_readonly_resummarize PROPERTIES TIMEOUT 10)
    add_test(NAME agc_driver_eop_interrupts COMMAND agc_driver_tests --eop-interrupts)
    set_tests_properties(agc_driver_eop_interrupts PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_readonly_resummarize agc_driver_eop_interrupts PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_small_copy_sync COMMAND agc_driver_tests --small-copy-sync)
    set_tests_properties(agc_driver_small_copy_sync PROPERTIES TIMEOUT 30 ENVIRONMENT "APS5_COPY_SYNC=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_small_copy_sync PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_mode_register COMMAND agc_shader_mode_register_tests)
    set_tests_properties(agc_shader_mode_register PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_shader_mode_register PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_bindless COMMAND agc_shader_memory_tests --bindless)
    set_tests_properties(agc_shader_bindless PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_selected_records_image_limit COMMAND agc_shader_memory_tests --selected-records-image-limit)
    add_test(NAME agc_shader_selected_records_pair_limit COMMAND agc_shader_memory_tests --selected-records-pair-limit)
    add_test(NAME agc_shader_selected_records_direct_image_limit COMMAND agc_shader_memory_tests --selected-records-direct-image-limit)
    add_test(NAME agc_shader_selected_records_direct_pair_limit COMMAND agc_shader_memory_tests --selected-records-direct-pair-limit)
    set_tests_properties(agc_shader_selected_records_image_limit agc_shader_selected_records_pair_limit agc_shader_selected_records_direct_image_limit agc_shader_selected_records_direct_pair_limit PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_srt_read_patching COMMAND agc_shader_memory_tests --srt-read-patching)
    set_tests_properties(agc_shader_srt_read_patching PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_table_snapshot_reuse COMMAND agc_shader_memory_tests --table-snapshot-reuse)
    set_tests_properties(agc_shader_table_snapshot_reuse PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_image_table_vouched_lookups COMMAND agc_driver_image_table_cache_bounds_tests --vouched-lookups)
    set_tests_properties(agc_driver_image_table_vouched_lookups PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_WRITE_WATCH_IMPORTS=watch" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)
    add_test(NAME agc_driver_image_table_vouch_queued_draws COMMAND agc_driver_image_table_cache_bounds_tests --queued-draws)
    set_tests_properties(agc_driver_image_table_vouch_queued_draws PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_shader_table_snapshot_reuse agc_driver_image_table_vouched_lookups agc_driver_image_table_vouch_queued_draws PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_type_zero_image_materialization COMMAND agc_shader_memory_tests --type-zero-image-materialization)
    set_tests_properties(agc_shader_type_zero_image_materialization PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_frontend_pair COMMAND agc_shader_memory_tests --frontend-pair)
    add_test(NAME agc_shader_quad_mask_no_demote COMMAND agc_shader_memory_tests --quad-mask-no-demote)
    set_tests_properties(agc_shader_quad_mask_no_demote PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_shader_quad_mask_no_demote PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_frontend_pair_off COMMAND agc_shader_memory_tests --frontend-pair)
    set_tests_properties(agc_shader_frontend_pair PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_shader_frontend_pair_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_FRONTEND_PAIR=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_layout_pair COMMAND agc_shader_memory_tests --layout-pair)
    add_test(NAME agc_shader_layout_pair_off COMMAND agc_shader_memory_tests --layout-pair)
    set_tests_properties(agc_shader_layout_pair PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_shader_layout_pair_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_LAYOUT_PAIR=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_entry_lane_proof_budget COMMAND agc_shader_memory_tests --entry-lane-proof-budget)
    set_tests_properties(agc_shader_entry_lane_proof_budget PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_shader_bindless agc_shader_selected_records_image_limit agc_shader_selected_records_pair_limit agc_shader_selected_records_direct_image_limit agc_shader_selected_records_direct_pair_limit agc_shader_srt_read_patching agc_shader_type_zero_image_materialization agc_shader_frontend_pair agc_shader_frontend_pair_off agc_shader_layout_pair agc_shader_layout_pair_off agc_shader_entry_lane_proof_budget PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(agcTest IN ITEMS agc_vertex_input_tests agc_mesh_output_budget_tests agc_shared_write_tests agc_locks_tests agc_driver_tests agc_shader_memory_tests agc_structurizer_tests agc_spirv_module_tests agc_dpp_write_tests agc_masked_select_tests agc_ir_reclaim_tests agc_vector_window_tests agc_constant_bit_test_tests agc_integer_narrowing_tests agc_scalar_select_mask_tests agc_vertex_input_validation_tests agc_vop1_control_decode_tests agc_validate_program_tests agc_pass_hazards_tests agc_barrier_batch_tests agc_shader_disk_cache_tests agc_screenshot_tests agc_shader_device_profile_tests agc_placeholder_images_tests agc_shader_control_flow_tests agc_driver_flip_tests agc_driver_pm4_tests agc_driver_graphics_tests agc_driver_bda_device_tests agc_driver_mesh_tests agc_driver_cmask_fast_clear_tests agc_formatted_buffer_once_tests agc_formatted_buffer_byte_stores_tests ${agcExecutionTests} agc_fma_mix_once_tests agc_f16_single_convert_tests)
        if(agcTest STREQUAL "agc_driver_constant_store_tests" OR agcTest STREQUAL "agc_driver_flat_apertures_tests" OR agcTest STREQUAL "agc_driver_pipeline_optimized_readback_tests" OR agcTest STREQUAL "agc_driver_rcp_hardware_tests")
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
    add_test(NAME agc_driver_guarded_direct_root COMMAND agc_driver_guarded_null_pointer_tests --direct-root-only)
    set_tests_properties(agc_driver_guarded_direct_root PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_guarded_direct_root PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_bda_device_selection_cpu COMMAND agc_driver_bda_device_tests --device-selection-cpu)
    set_tests_properties(agc_driver_bda_device_selection_cpu PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_bda_device_selection_cpu PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_draw_unsupported_buffer_type_skip_refused COMMAND agc_driver_draw_unsupported_buffer_type_tests --skip-refused)
    set_tests_properties(agc_driver_draw_unsupported_buffer_type_skip_refused PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SKIP_REFUSED_DRAWS=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_unsupported_buffer_type_skip_refused PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_period_copy_no_hle COMMAND agc_driver_period_copy_tests)
    set_tests_properties(agc_driver_period_copy_no_hle PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_COPY_HLE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_period_copy_no_hle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_sparse_indirect_indices_unreadable COMMAND agc_driver_sparse_indirect_indices_tests --unreadable)
    set_tests_properties(agc_driver_sparse_indirect_indices agc_driver_sparse_indirect_indices_unreadable PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sparse_indirect_indices_unreadable PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_pixel_implicit_derivatives PROPERTIES TIMEOUT 30)
    set_tests_properties(agc_driver_storage_file_write PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SUSPEND_DRAIN=1")
    set_tests_properties(agc_driver_clamped_global_fill PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_LABEL_READBACK_STORE=1")
    set_tests_properties(agc_driver_draw_pipeline_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" TIMEOUT 30)
    set_property(TEST agc_driver_draw_pipeline_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_draw_pipeline_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_draw_pipeline_trace_tests> disabled)
    set_tests_properties(agc_driver_draw_pipeline_trace_disabled PROPERTIES ENVIRONMENT "APS5_PIPELINED_DRAWS=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_driver_draw_finisher_trace COMMAND agc_driver_draw_pipeline_trace_tests finisher)
    set_tests_properties(agc_driver_draw_finisher_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_draw_finisher_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_draw_finisher_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_draw_pipeline_trace_tests> finisher-disabled)
    set_tests_properties(agc_driver_draw_finisher_trace_disabled PROPERTIES ENVIRONMENT "APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_draw_pipeline_trace_disabled agc_driver_draw_finisher_trace agc_driver_draw_finisher_trace_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_copied_writer_label_order_drain COMMAND agc_driver_copied_writer_label_order_tests)
    set_tests_properties(agc_driver_copied_writer_label_order_drain PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAIN_COMPLETION_LABELS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_copied_writer_label_order_drain PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_lazy_resolved_abi_switch_set COMMAND agc_driver_lazy_resolved_abi_tests)
    set_tests_properties(agc_driver_lazy_resolved_abi_switch_set PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_LAZY_SHADER_PREPARE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_lazy_resolved_abi_switch_set PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_queued_raw_shader PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8" TIMEOUT 30)
    set(agcDrawPipelineTests agc_driver_draw_write_ranges agc_driver_draw_unsupported_buffer_type agc_driver_bda_span_reads agc_driver_copied_writer_copy agc_driver_image_table_reuse agc_driver_depth_storage_reuse agc_driver_staged_clear_sampled_read agc_driver_storage_write_back_overhang agc_driver_write_data_written_ahead agc_driver_gated_dispatch_capture agc_driver_buffer_load_store agc_driver_pipelined_dma agc_driver_pipelined_fill agc_driver_pipelined_predication_read agc_driver_pipelined_predicated_label_order agc_driver_pipelined_metadata_pass agc_driver_draw_cache_reuse agc_driver_draw_cache_relocation agc_driver_draw_data_hit_user_words agc_driver_finisher_label_order agc_driver_same_queue_label_wait agc_driver_unimported_label_order)
    foreach(agcTestName IN LISTS agcDrawPipelineTests)
        add_test(NAME ${agcTestName}_draw_pipeline COMMAND ${agcTestName}_tests)
        set_tests_properties(${agcTestName}_draw_pipeline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(${agcTestName}_draw_pipeline PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_driver_partially_resident_pending_stores COMMAND agc_driver_partially_resident_snapshot_tests --pending-stores)
    set_tests_properties(agc_driver_multisample_readback PROPERTIES TIMEOUT 20)
    foreach(agcReadbackCase IN ITEMS uint8 uint16 uint64 uint128 depth upload retile depth_retile)
        add_test(NAME agc_driver_multisample_readback_${agcReadbackCase} COMMAND agc_driver_multisample_readback_tests ${agcReadbackCase})
        set_tests_properties(agc_driver_multisample_readback_${agcReadbackCase} PROPERTIES TIMEOUT 20 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_multisample_readback_${agcReadbackCase} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_partially_resident_pending_stores PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_partially_resident_pending_stores PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_shader_disk_cache_large_file COMMAND agc_shader_disk_cache_tests --large-file)
    if(Python3_Interpreter_FOUND)
        set(housekeepingModes plain)
        if(WIN32)
            list(APPEND housekeepingModes extended extended-long device)
        else()
            list(APPEND housekeepingModes plain-long)
        endif()
        foreach(housekeepingMode IN LISTS housekeepingModes)
            set(housekeepingTest agc_shader_cache_housekeeping_${housekeepingMode})
            add_test(NAME ${housekeepingTest} COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shader_cache_housekeeping.py
                $<TARGET_FILE:agc_shader_disk_cache_tests> ${housekeepingMode})
            set_tests_properties(${housekeepingTest} PROPERTIES TIMEOUT 20)
            if(WIN32)
                set_tests_properties(${housekeepingTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endif()
    set_tests_properties(agc_shader_disk_cache_large_file PROPERTIES SKIP_RETURN_CODE 77 RUN_SERIAL TRUE WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 240)
    if(WIN32)
        set_tests_properties(agc_shader_disk_cache_large_file PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_shader_disk_cache_long_paths COMMAND agc_shader_disk_cache_tests --long-host-paths)
        add_test(NAME agc_shader_disk_cache_host_stderr COMMAND agc_shader_disk_cache_tests --host-stderr)
        set_tests_properties(agc_shader_disk_cache_host_stderr PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_shader_disk_cache_long_paths PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(Python3_Interpreter_FOUND)
        set(prebuiltDiscoveryModes plain relative plain-long)
        if(WIN32)
            list(APPEND prebuiltDiscoveryModes extended extended-forward device extended-long)
        endif()
        foreach(prebuiltDiscoveryMode IN LISTS prebuiltDiscoveryModes)
            set(prebuiltDiscoveryTest agc_shader_prebuilt_discovery_${prebuiltDiscoveryMode})
            add_test(NAME ${prebuiltDiscoveryTest} COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shader_cache_prebuilt_discovery.py
                $<TARGET_FILE:agc_shader_disk_cache_tests> ${prebuiltDiscoveryMode})
            set_tests_properties(${prebuiltDiscoveryTest} PROPERTIES TIMEOUT 20)
            if(WIN32)
                set_tests_properties(${prebuiltDiscoveryTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endif()
    add_test(NAME agc_driver_large_tracked_snapshot COMMAND agc_driver_partially_resident_snapshot_tests --large-tracked)
    set_tests_properties(agc_driver_large_tracked_snapshot PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_large_tracked_snapshot PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        add_test(NAME agc_driver_untracked_snapshot_hint COMMAND agc_driver_partially_resident_snapshot_tests --untracked-hint)
        set_tests_properties(agc_driver_untracked_snapshot_hint PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_registration_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" TIMEOUT 30)
    set_property(TEST agc_driver_registration_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_registration_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_registration_trace_tests> disabled)
    set_tests_properties(agc_driver_registration_trace_disabled PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_registration_trace_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_sparse_buffer_holes COMMAND agc_driver_graphics_tests --sparse-buffer-holes)
    add_test(NAME agc_copy_input_kept_bytes COMMAND agc_driver_graphics_tests --copy-input-kept-bytes)
    add_test(NAME agc_depth_copy_kept_bytes COMMAND agc_driver_graphics_tests --depth-copy-kept-bytes)
    set_tests_properties(agc_copy_input_kept_bytes agc_depth_copy_kept_bytes PROPERTIES
            TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
            ENVIRONMENT_MODIFICATION "APS5_CPU_COPIES=unset:;APS5_GPU_COPY_MAX_KIB=unset:;APS5_CAPTURE_GPU_COPIES=unset:;APS5_BUFFER_POOL_SLOTS=unset:;APS5_BUFFER_POOL_SHARED=unset:;APS5_NO_BUFFER_CLASSES=unset:;APS5_STAGING_POOL_MIB=unset:")
    if(WIN32)
        set_property(TEST agc_copy_input_kept_bytes APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_property(TEST agc_depth_copy_kept_bytes APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_native_multisample_depth PROPERTIES TIMEOUT 30)
    set_tests_properties(agc_driver_sample_order PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_order_grid_reference COMMAND agc_driver_sample_order_tests --grid-reference)
    add_test(NAME agc_driver_sample_order_recipe_depth_locations COMMAND agc_driver_sample_order_tests --recipe-depth-locations)
    set_tests_properties(agc_driver_sample_order_grid_reference agc_driver_sample_order_recipe_depth_locations PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sample_order_grid_reference agc_driver_sample_order_recipe_depth_locations PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_native_depth_retirement COMMAND agc_driver_native_multisample_depth_tests --retirement)
    set_tests_properties(agc_driver_native_depth_retirement PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_native_depth_clears COMMAND agc_driver_native_multisample_depth_tests --clears)
    add_test(NAME agc_driver_native_depth_clear_refusals COMMAND agc_driver_native_multisample_depth_tests --clear-refusals)
    set_tests_properties(agc_driver_native_depth_clears agc_driver_native_depth_clear_refusals PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_native_depth_retirement PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_native_depth_clears agc_driver_native_depth_clear_refusals PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_sparse_buffer_holes PROPERTIES TIMEOUT 30)
    add_test(NAME agc_pipeline_journal COMMAND agc_driver_graphics_tests --pipeline-journal)
    add_test(NAME agc_pipeline_journal_long_paths COMMAND agc_driver_graphics_tests --pipeline-journal-long-paths)
    set_tests_properties(agc_pipeline_journal_long_paths PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_pipeline_library_clear_waits_for_build COMMAND agc_driver_graphics_tests --pipeline-library-clear-waits-for-build)
    add_test(NAME agc_pipeline_store_lazy_buffer_pool COMMAND agc_driver_graphics_tests --pipeline-store-lazy-buffer-pool)
    set_tests_properties(agc_pipeline_journal PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_pipeline_library_clear_waits_for_build PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_pipeline_store_lazy_buffer_pool PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        add_test(NAME agc_pipeline_journal_extended_paths COMMAND agc_driver_graphics_tests --pipeline-journal-extended-paths)
        set_tests_properties(agc_pipeline_journal_extended_paths PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        set_tests_properties(agc_pipeline_journal_long_paths agc_pipeline_journal_extended_paths PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_pipeline_journal agc_pipeline_library_clear_waits_for_build agc_pipeline_store_lazy_buffer_pool PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_sparse_buffer_holes PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_sparse_buffer_holes PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_heap_mirror_unwatch COMMAND agc_driver_graphics_tests --heap-mirror-unwatch)
    add_test(NAME agc_heap_mirror_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_unwatch COMMAND agc_driver_graphics_tests --sparse-heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --sparse-heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_budget COMMAND agc_driver_graphics_tests --sparse-heap-mirror-budget)
    add_test(NAME agc_heap_mirror_run_unwatch COMMAND agc_driver_graphics_tests --heap-mirror-run-unwatch)
    add_test(NAME agc_heap_mirror_run_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --heap-mirror-run-unwatch)
    set_tests_properties(agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch_no_space_cache PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_SPACE_CACHE=1")
    set_tests_properties(agc_heap_mirror_unwatch agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch agc_heap_mirror_run_unwatch_no_space_cache agc_sparse_heap_mirror_budget PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_heap_mirror_unwatch agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch agc_heap_mirror_run_unwatch_no_space_cache agc_sparse_heap_mirror_budget PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    else()
        add_test(NAME agc_heap_mirror_unwatched_import COMMAND agc_driver_graphics_tests --heap-mirror-unwatched-import)
        set_tests_properties(agc_heap_mirror_unwatched_import PROPERTIES TIMEOUT 30)
    endif()
    add_test(NAME agc_driver_lds_lock_budget COMMAND agc_driver_lds_mixed_width_atomics_tests --lock-budget)
    set_tests_properties(agc_driver_lds_lock_budget PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_lds_lock_budget PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test(NAME agc_driver_emulated_color_compare_srgb_decode COMMAND agc_driver_emulated_color_compare_tests)
    set_tests_properties(agc_driver_emulated_color_compare_srgb_decode PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SRGB_SHADER_DECODE=1"
            SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_emulated_color_compare agc_driver_emulated_color_compare_srgb_decode PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_emulated_color_compare_srgb_decode PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_image_srgb_sample_copy_forced COMMAND agc_driver_image_srgb_sample_copy_tests)
    set_tests_properties(agc_driver_image_srgb_sample_copy_forced PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SRGB_SHADER_DECODE=copy"
            SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_image_srgb_sample_copy_forced PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME agc_driver_graphics_maps_fallback COMMAND agc_driver_graphics_tests)
        set_tests_properties(agc_driver_graphics_maps_fallback PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PROCMAP_QUERY=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
    add_test(NAME agc_shared_write_partial_collect COMMAND agc_shared_write_tests --partial-collect)
    set_tests_properties(agc_shared_write_partial_collect PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        add_test(NAME agc_shared_write_partial_collect_two_pass COMMAND agc_shared_write_tests --partial-collect)
        set_tests_properties(agc_shared_write_partial_collect_two_pass PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_SINGLE_PASS_COLLECT=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        set_tests_properties(agc_shared_write_partial_collect agc_shared_write_partial_collect_two_pass PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_target_properties(agc_driver_pipeline_optimized_readback_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(pipelineReadbackCase IN ITEMS d32 d16 color32 color64)
        foreach(pipelineReadbackMode IN ITEMS default fast)
            set(pipelineReadbackTest agc_driver_pipeline_optimized_readback_${pipelineReadbackCase})
            set(pipelineReadbackEnvironment "ANYPS5_NO_SHADER_CACHE=1")
            if(pipelineReadbackMode STREQUAL "fast")
                string(APPEND pipelineReadbackTest "_fast")
                list(APPEND pipelineReadbackEnvironment "APS5_NO_PIPELINE_LTO=1")
            endif()
            add_test(NAME ${pipelineReadbackTest} COMMAND agc_driver_pipeline_optimized_readback_tests ${pipelineReadbackCase})
            set_tests_properties(${pipelineReadbackTest} PROPERTIES ENVIRONMENT "${pipelineReadbackEnvironment}" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
            if(WIN32)
                set_tests_properties(${pipelineReadbackTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    add_test(NAME agc_vector_window_stage_inputs COMMAND agc_vector_window_tests --stage-inputs-only)
    set_tests_properties(agc_vector_window_stage_inputs PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_vector_window_stage_inputs agc_driver_relative_stage_inputs PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_vector_window_stage_inputs PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_layered_volume_vertex COMMAND agc_driver_layered_volume_draw_tests --vertex-only)
    add_test(NAME agc_driver_layered_volume_vertex_no_gpl COMMAND agc_driver_layered_volume_draw_tests --vertex-only)
    set_tests_properties(agc_driver_layered_volume_vertex agc_driver_layered_volume_vertex_no_gpl PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(TEST agc_driver_layered_volume_vertex_no_gpl APPEND PROPERTY ENVIRONMENT "APS5_NO_GPL=1")
    if(WIN32)
        set_tests_properties(agc_driver_layered_volume_vertex agc_driver_layered_volume_vertex_no_gpl PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_f64_denorm_preserve_spirv COMMAND agc_driver_f64_fma_subnormals_tests --verify-spv-denorm-mode)
    set_tests_properties(agc_driver_f64_denorm_preserve_spirv PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_f64_denorm_preserve_spirv PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_f16_to_f32_unpack_spirv COMMAND agc_driver_f16_to_f32_unpack_tests --verify-spirv)
    set_tests_properties(agc_driver_f16_to_f32_unpack_spirv PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_f16_to_f32_unpack_spirv PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_structurizer_intersection_off COMMAND agc_structurizer_tests)
    set_tests_properties(agc_structurizer_intersection_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_DOM_INTERSECTION=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_structurizer_iteration_selection_off COMMAND agc_structurizer_tests)
    set_tests_properties(agc_structurizer_iteration_selection_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_ITERATION_SELECTION=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_control_flow_iteration_selection_off COMMAND agc_shader_control_flow_tests)
    set_tests_properties(agc_shader_control_flow_iteration_selection_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PERF_ITERATION_SELECTION=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_structurizer_forced_fallback COMMAND agc_structurizer_tests)
    set_tests_properties(agc_structurizer_forced_fallback PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FORCE_STRUCTURIZER_FALLBACK=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_shader_control_flow_forced_fallback COMMAND agc_shader_control_flow_tests)
    set_tests_properties(agc_shader_control_flow_forced_fallback PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FORCE_STRUCTURIZER_FALLBACK=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_structurizer_intersection_off agc_structurizer_iteration_selection_off agc_shader_control_flow_iteration_selection_off agc_structurizer_forced_fallback agc_shader_control_flow_forced_fallback PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_gated_dispatch_capture_short_timeout COMMAND agc_driver_gated_dispatch_capture_tests)
    set_tests_properties(agc_driver_gated_dispatch_capture_short_timeout PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_WAIT_TIMEOUT_MS=1;APS5_NO_PIPELINE_LTO=1"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_gated_dispatch_capture_short_timeout PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_provoking_vertex PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_dword_pattern_fill_translated COMMAND agc_driver_dword_pattern_fill_tests)
    set_tests_properties(agc_driver_dword_pattern_fill_translated PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_FILL_HLE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_dword_pattern_fill_translated PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_uint8x4_color_target_no_resident COMMAND agc_driver_uint8x4_color_target_tests)
    set_tests_properties(agc_driver_uint8x4_color_target_no_resident PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_RESIDENT_TARGETS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_uint8x4_color_target_no_resident PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set(agcGuestMemoryLifetimeTests agc_driver_guest_memory_lifetime)
    foreach(mode IN ITEMS copy direct locked heap heap_uncached)
        set(testName agc_driver_guest_memory_lifetime_${mode})
        if(mode STREQUAL "heap_uncached")
            add_test(NAME ${testName} COMMAND agc_driver_guest_memory_lifetime_tests heap)
        else()
            add_test(NAME ${testName} COMMAND agc_driver_guest_memory_lifetime_tests ${mode})
        endif()
        list(APPEND agcGuestMemoryLifetimeTests ${testName})
    endforeach()
    set_tests_properties(${agcGuestMemoryLifetimeTests} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000;APS5_HEAP_CACHE_MIB=4"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_guest_memory_lifetime_heap_uncached PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000;APS5_HEAP_CACHE_MIB=0")
    if(WIN32)
        set_tests_properties(${agcGuestMemoryLifetimeTests} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND Python3_Interpreter_FOUND)
        add_library(agc_guest_memory_lifetime_fixture SHARED tests/GuestMemoryLifetimeFixture.cpp)
        target_include_directories(agc_guest_memory_lifetime_fixture PRIVATE
                ${LIBS_INCLUDE_DIR}
                ${CMAKE_SOURCE_DIR}/core/shader/recompiler
                ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
                ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include)
        target_link_libraries(agc_guest_memory_lifetime_fixture PRIVATE libSceAgcDriver SDL2-static libc)
        add_dependencies(agc_guest_memory_lifetime_fixture relinker libkernel)
        set_target_properties(agc_guest_memory_lifetime_fixture PROPERTIES
                OUTPUT_NAME GuestMemoryFixture SUFFIX ".prx" CXX_EXTENSIONS OFF
                BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN")
        add_test(NAME agc_driver_guest_memory_lifetime_converted COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_guest_memory_lifetime.py
                ${CMAKE_SOURCE_DIR}/core/relinker/relinker/tests
                $<TARGET_FILE:relinker>
                $<TARGET_FILE:agc_guest_memory_lifetime_fixture>
                $<TARGET_FILE:libkernel>
                $<TARGET_FILE:libc>
                $<TARGET_FILE:libSceAgcDriver>)
        set_tests_properties(agc_driver_guest_memory_lifetime_converted PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000" SKIP_RETURN_CODE 77 TIMEOUT 30)
    endif()
    set_tests_properties(agc_driver_flat_store PROPERTIES RUN_SERIAL TRUE)
    set_target_properties(agc_driver_flat_apertures_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(flatAperturesPart RANGE 13)
        add_test(NAME agc_driver_flat_apertures_${flatAperturesPart} COMMAND agc_driver_flat_apertures_tests --part ${flatAperturesPart}/14)
        set_tests_properties(agc_driver_flat_apertures_${flatAperturesPart} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_flat_apertures_${flatAperturesPart} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_storage_cache_budget PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_storage_cache_retained_batch COMMAND agc_driver_storage_cache_budget_tests --retained-batch)
    set_tests_properties(agc_driver_storage_cache_retained_batch PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_storage_cache_retained_batch PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_sampled_cache_budget_memory COMMAND agc_driver_sampled_cache_budget_tests memory)
    set_tests_properties(agc_driver_sampled_cache_budget_memory PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_sampled_cache_budget_without_storage_image_multisample COMMAND agc_driver_sampled_cache_budget_tests without-storage-image-multisample)
    set_tests_properties(agc_driver_sampled_cache_budget_without_storage_image_multisample PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sampled_cache_device_selection COMMAND agc_driver_sampled_cache_budget_tests device-selection-cpu)
    set_tests_properties(agc_sampled_cache_device_selection PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_driver_sampled_cache_device_selection COMMAND agc_driver_sampled_cache_budget_tests device-selection)
    set_tests_properties(agc_driver_sampled_cache_device_selection PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_sampled_cache_device_selection agc_driver_sampled_cache_device_selection PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(imageRecoveryMode IN ITEMS sampled storage srgb repack)
        add_test(NAME agc_driver_recover_${imageRecoveryMode}_allocation COMMAND agc_driver_sampled_cache_budget_tests recover-${imageRecoveryMode})
        set_tests_properties(agc_driver_recover_${imageRecoveryMode}_allocation PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_driver_recover_${imageRecoveryMode}_allocation PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_property(TEST agc_driver_recover_srgb_allocation APPEND PROPERTY ENVIRONMENT_MODIFICATION "APS5_SRGB_SHADER_DECODE=set:copy")
    add_test(NAME agc_driver_resource_cache_storage_admission COMMAND agc_driver_sampled_cache_budget_tests resource-admission)
    add_test(NAME agc_driver_resource_cache_oversized_storage_admission COMMAND agc_driver_sampled_cache_budget_tests resource-oversized)
    add_test(NAME agc_driver_resource_cache_storage_clear COMMAND agc_driver_sampled_cache_budget_tests resource-clear)
    add_test(NAME agc_driver_resource_cache_replaced_texture COMMAND agc_driver_sampled_cache_budget_tests resource-replaced)
    add_test(NAME agc_driver_resource_cache_lookup_changed COMMAND agc_driver_sampled_cache_budget_tests resource-lookup-changed)
    set_tests_properties(agc_driver_resource_cache_storage_admission agc_driver_resource_cache_oversized_storage_admission agc_driver_resource_cache_storage_clear agc_driver_resource_cache_replaced_texture agc_driver_resource_cache_lookup_changed PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_sampled_cache_budget_memory agc_driver_sampled_cache_budget_without_storage_image_multisample agc_driver_resource_cache_storage_admission agc_driver_resource_cache_oversized_storage_admission agc_driver_resource_cache_storage_clear agc_driver_resource_cache_replaced_texture agc_driver_resource_cache_lookup_changed PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    # The driver draw test again with the draw key off: the draw cache then keys on the decoded vertex
    # stage, which must include PA_CL_VS_OUT_CNTL.
    add_test(NAME agc_driver_vertex_clip_cull_draw_no_key COMMAND agc_driver_vertex_clip_cull_draw_tests no-key)
    set_tests_properties(agc_driver_vertex_clip_cull_draw_no_key PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_KEY=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_vertex_clip_cull_draw_no_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_pm4_failure COMMAND agc_driver_pm4_tests failure)
    set_tests_properties(agc_driver_pm4_failure PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set(pm4RingCases "ring|APS5_PM4_RING=4" "ring-empty|APS5_PM4_RING=16" "ring-disabled|APS5_PM4_RING=0" "ring-invalid|APS5_PM4_RING=4x" "ring-report|APS5_PM4_RING=2" "stall-watchdog|APS5_STALL_WATCHDOG_SECS=1"
            "stall-watchdog-invalid|APS5_STALL_WATCHDOG_SECS=1x" "stall-watchdog-rewind|APS5_STALL_WATCHDOG_SECS=1" "stall-watchdog-order|APS5_STALL_WATCHDOG_SECS=1")
    if(WIN32)
        list(APPEND pm4RingCases "ring-report-heap|APS5_PM4_RING=2")
    endif()
    foreach(pm4RingCase IN LISTS pm4RingCases)
        string(REPLACE "|" ";" pm4RingCase "${pm4RingCase}")
        list(GET pm4RingCase 0 pm4RingMode)
        list(GET pm4RingCase 1 pm4RingEnvironment)
        string(REPLACE "-" "_" pm4RingTest "agc_driver_pm4_${pm4RingMode}")
        add_test(NAME ${pm4RingTest} COMMAND agc_driver_pm4_tests ${pm4RingMode})
        set_tests_properties(${pm4RingTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;${pm4RingEnvironment}" SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(${pm4RingTest} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_descriptor_pool_reset_failure COMMAND agc_driver_graphics_tests --descriptor-pool-reset-failure)
    add_test(NAME agc_descriptor_cache COMMAND agc_driver_graphics_tests --descriptor-cache)
    add_test(NAME agc_driver_address_space_teardown COMMAND agc_driver_graphics_tests --address-space-teardown)
    set_tests_properties(agc_driver_address_space_teardown PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_pm4_rewind_trace COMMAND agc_driver_pm4_tests rewind-trace)
    set_tests_properties(agc_driver_pm4_rewind_trace PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_DIR=${CMAKE_CURRENT_BINARY_DIR}/pm4-rewind-trace" TIMEOUT 30)
    set_property(TEST agc_driver_pm4_rewind_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    if(WIN32)
        set_property(TEST agc_driver_pm4_rewind_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_pm4_failure_inline COMMAND agc_driver_pm4_tests failure)
    set_tests_properties(agc_driver_pm4_failure_inline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pm4_failure agc_driver_pm4_failure_inline agc_descriptor_pool_reset_failure agc_descriptor_cache agc_driver_address_space_teardown PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(NOT WIN32)
        add_test(NAME agc_driver_pm4_priority_refused COMMAND agc_driver_pm4_tests priority-refused)
        set_tests_properties(agc_driver_pm4_priority_refused PROPERTIES ENVIRONMENT
            "ANYPS5_NO_SHADER_CACHE=1;APS5_THREAD_PRIORITY=high;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/aps5-no-rtkit" SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME agc_driver_present_mode COMMAND agc_driver_flip_tests present-mode)
    add_test(NAME agc_driver_worker_failure_log COMMAND agc_driver_flip_tests failure-log)
    set_tests_properties(agc_driver_worker_failure_log PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 20)
    add_test(NAME agc_driver_flip_compute COMMAND agc_driver_flip_tests compute)
    set_tests_properties(agc_driver_flip_compute PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_flip_compute agc_driver_present_mode agc_driver_worker_failure_log PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_linear_readback_label_off COMMAND agc_driver_linear_readback_label_tests)
    set_tests_properties(agc_driver_linear_readback_label_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_LABEL_READBACK_STORE=1" WILL_FAIL TRUE TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_linear_readback_label_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_draw_cache_reuse_off COMMAND agc_driver_draw_cache_reuse_tests)
    set_tests_properties(agc_driver_draw_cache_reuse_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPL=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_draw_cache_reuse_verify COMMAND agc_driver_draw_cache_reuse_tests)
    set_tests_properties(agc_driver_draw_cache_reuse_verify PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_DATA_HITS=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_draw_cache_reuse_entry_verify COMMAND agc_driver_draw_cache_reuse_tests)
    set_tests_properties(agc_driver_draw_cache_reuse_entry_verify PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_ENTRIES=1" SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:")
    if(WIN32)
        set_tests_properties(agc_driver_draw_cache_reuse_entry_verify PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_draw_cache_reuse_inline COMMAND agc_driver_draw_cache_reuse_tests)
    set_tests_properties(agc_driver_draw_cache_reuse_inline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0;APS5_VERIFY_DRAW_RECIPE=1;APS5_VERIFY_REGISTER_KEYS=1" SKIP_RETURN_CODE 77)
    set_tests_properties(agc_driver_draw_cache_reuse agc_driver_draw_cache_reuse_off agc_driver_draw_cache_reuse_verify agc_driver_draw_cache_reuse_inline PROPERTIES FAIL_REGULAR_EXPRESSION "FATAL:")
    set_property(TEST agc_driver_draw_cache_reuse agc_driver_draw_cache_reuse_off APPEND PROPERTY ENVIRONMENT "APS5_VERIFY_DRAW_RECIPE=1" "APS5_VERIFY_REGISTER_KEYS=1")
    add_test(NAME agc_driver_draw_data_hit_user_words_verify COMMAND agc_driver_draw_data_hit_user_words_tests)
    set_tests_properties(agc_driver_draw_data_hit_user_words_verify PROPERTIES ENVIRONMENT "APS5_VERIFY_DRAW_DATA_HITS=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_draw_data_hit_user_words_refresh COMMAND agc_driver_draw_data_hit_user_words_tests)
    set_tests_properties(agc_driver_draw_data_hit_user_words_refresh PROPERTIES ENVIRONMENT "APS5_DRAW_PIPELINE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_draw_cache_reuse_off agc_driver_draw_cache_reuse_verify agc_driver_draw_cache_reuse_inline agc_driver_draw_data_hit_user_words_verify agc_driver_draw_data_hit_user_words_refresh PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test(NAME agc_driver_stencil_replay_no_gpl COMMAND agc_driver_stencil_replay_tests)
    set_tests_properties(agc_driver_stencil_replay_no_gpl PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPL=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_stencil_replay_no_gpl PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_pipelined_dispatch_item COMMAND agc_driver_pipelined_dispatch_tests)
    set_tests_properties(agc_driver_pipelined_dispatch_item PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINE_DISPATCH=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pipelined_dispatch_item PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_queued_index_read_order PROPERTIES TIMEOUT 60)

    foreach(agcTest IN ITEMS agc_spirv_narrow_constant_store_tests agc_spirv13_image_tests agc_spirv_optimizer_tests agc_driver_constant_store_tests)
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
            if(agcTest STREQUAL "agc_spirv13_image_tests")
                set_tests_properties(${agcTestName} PROPERTIES TIMEOUT 30)
            endif()
            if(agcTest STREQUAL "agc_driver_constant_store_tests")
                set_tests_properties(${agcTestName} PROPERTIES RUN_SERIAL TRUE)
            endif()
            if(WIN32)
                set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    add_test(NAME agc_driver_array_layer_target_cpu COMMAND agc_driver_array_layer_target_tests)
    set_tests_properties(agc_driver_array_layer_target_cpu PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_HOST_IMPORT=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_array_layer_target_cpu PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(preparedMode IN ITEMS indirect uncached debug debug_failure deferred deferred_undecodable registered_workgroup registered_workgroup_sync dispatch_ahead_prewarm abi_deferred_compute abi_deferred_pixel abi_deferred_stages abi_deferred_rectangle abi_deferred_undecodable_state abi_deferred_registration_failure abi_deferred_vertex_inputs abi_deferred_rectangle_pairs lazy lazy_sync)
        set(preparedTest "agc_driver_prepared_shaders_${preparedMode}")
        if(preparedMode MATCHES "^abi_deferred_")
            string(REPLACE "_" "-" preparedArgument ${preparedMode})
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --${preparedArgument})
        elseif(preparedMode STREQUAL "indirect")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --indirect)
        elseif(preparedMode STREQUAL "deferred")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --deferred)
        elseif(preparedMode STREQUAL "deferred_undecodable")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --deferred-undecodable)
        elseif(preparedMode STREQUAL "lazy" OR preparedMode STREQUAL "lazy_sync")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --lazy)
        elseif(preparedMode STREQUAL "registered_workgroup" OR preparedMode STREQUAL "registered_workgroup_sync")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --registered-workgroup)
        elseif(preparedMode STREQUAL "dispatch_ahead_prewarm")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --dispatch-ahead-prewarm)
        elseif(preparedMode STREQUAL "debug_failure")
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests --fail-before-registration)
            set_tests_properties(${preparedTest} PROPERTIES WILL_FAIL TRUE TIMEOUT 30)
        else()
            add_test(NAME ${preparedTest} COMMAND agc_driver_prepared_shaders_tests)
        endif()
        set_tests_properties(${preparedTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(preparedMode MATCHES "^abi_deferred_")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_NO_FAILURE_MEMO=1")
        elseif(preparedMode STREQUAL "uncached")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_NO_DISPATCH_CACHE=1")
        elseif(preparedMode STREQUAL "registered_workgroup_sync")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_SYNC_SHADER_PREPARE=1")
        elseif(preparedMode STREQUAL "lazy" OR preparedMode STREQUAL "lazy_sync")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_LAZY_SHADER_PREPARE=1" "APS5_SUSPEND_DRAIN=1")
            if(preparedMode STREQUAL "lazy_sync")
                set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_SYNC_SHADER_PREPARE=1")
            endif()
        elseif(preparedMode STREQUAL "debug" OR preparedMode STREQUAL "debug_failure")
            set_property(TEST ${preparedTest} APPEND PROPERTY ENVIRONMENT "APS5_PROFILE_DRAW=1" "APS5_NO_UNLOCKED_DEVICE=1")
        endif()
        if(WIN32)
            set_tests_properties(${preparedTest} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_driver_shutdown_threads_joined COMMAND agc_driver_shutdown_threads_tests --joined)
    add_test(NAME agc_driver_shutdown_threads_queued_prebuilds COMMAND agc_driver_shutdown_threads_tests --queued-prebuilds)
    add_test(NAME agc_driver_shutdown_threads_compare_helpers COMMAND agc_driver_shutdown_threads_tests --compare-helpers)
    add_test(NAME agc_driver_shutdown_threads_draw_committer COMMAND agc_driver_shutdown_threads_tests --draw-committer)
    add_test(NAME agc_driver_shutdown_threads_shader_cache_writer COMMAND agc_driver_shutdown_threads_tests --shader-cache-writer)
    add_test(NAME agc_driver_shutdown_threads_late_shader_cache COMMAND agc_driver_shutdown_threads_tests --late-shader-cache)
    add_test(NAME agc_driver_shutdown_threads_async_log_held COMMAND agc_driver_shutdown_threads_tests --async-log-held)
    add_test(NAME agc_driver_shutdown_threads_exit_after_start COMMAND agc_driver_shutdown_threads_tests --exit-after-start)
    set_tests_properties(agc_driver_shutdown_threads_joined agc_driver_shutdown_threads_queued_prebuilds agc_driver_shutdown_threads_compare_helpers agc_driver_shutdown_threads_draw_committer agc_driver_shutdown_threads_shader_cache_writer agc_driver_shutdown_threads_late_shader_cache agc_driver_shutdown_threads_async_log_held agc_driver_shutdown_threads_exit_after_start PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_shutdown_threads_joined agc_driver_shutdown_threads_queued_prebuilds agc_driver_shutdown_threads_compare_helpers agc_driver_shutdown_threads_draw_committer agc_driver_shutdown_threads_shader_cache_writer agc_driver_shutdown_threads_late_shader_cache agc_driver_shutdown_threads_async_log_held agc_driver_shutdown_threads_exit_after_start PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_prepared_shaders_abi_deferred_concurrent_lazy COMMAND agc_driver_prepared_shaders_tests --abi-deferred-concurrent)
    set_tests_properties(agc_driver_prepared_shaders_abi_deferred_concurrent_lazy PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_FAILURE_MEMO=1;APS5_LAZY_SHADER_PREPARE=1;APS5_SUSPEND_DRAIN=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_prepared_shaders_abi_deferred_concurrent_lazy PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_prepared_shaders_abi_deferred_compute_lazy COMMAND agc_driver_prepared_shaders_tests --abi-deferred-compute)
    set_tests_properties(agc_driver_prepared_shaders_abi_deferred_compute_lazy PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_FAILURE_MEMO=1;APS5_LAZY_SHADER_PREPARE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_prepared_shaders_abi_deferred_compute_lazy PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_compute_user_data_calls_registered COMMAND agc_driver_compute_user_data_calls_tests --registered)
    set_tests_properties(agc_driver_compute_user_data_calls_registered PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_compute_user_data_calls_registered PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
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
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )

    target_link_libraries(agc_driver_visual_test PRIVATE
            shader_recompiler_agc_driver
            decoder_png
            SDL2-static
            libc
            libkernel
            jpeg_turbo
    )
    add_dependencies(agc_driver_visual_test agc_trace_revision)

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
    foreach(optimizer IN ITEMS none full)
        if(optimizer STREQUAL "full" AND NOT ANYPS5_ENABLE_SPIRV_TOOLS)
            continue()
        endif()
        add_test(NAME agc_shader_specialized_bda_abi_${optimizer} COMMAND agc_shader_memory_tests --specialized-bda-abi)
        set_tests_properties(agc_shader_specialized_bda_abi_${optimizer} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SPIRV_OPT=${optimizer}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_shader_specialized_bda_abi_${optimizer} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test_executable(agc_shader_replay_requests_tests tests/ShaderReplayRequests.cpp)
    target_link_libraries(agc_shader_replay_requests_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_shader_replay_requests_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_shader_replay_requests_tests)
    add_dependencies(agc_shader_replay_requests_tests agc_shader_replay)
    set(agcShaderReplayCapture "${CMAKE_CURRENT_BINARY_DIR}/shader-replay-capture")
    add_test(NAME agc_shader_replay_requests COMMAND agc_shader_replay_requests_tests "${agcShaderReplayCapture}")
    set_tests_properties(agc_shader_replay_requests PROPERTIES FIXTURES_SETUP agc_shader_replay_capture TIMEOUT 30)
    add_test(NAME agc_shader_replay_tool COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:agc_shader_replay> "-DCAPTURE=${agcShaderReplayCapture}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ShaderReplayTool.cmake)
    set_tests_properties(agc_shader_replay_tool PROPERTIES FIXTURES_REQUIRED agc_shader_replay_capture TIMEOUT 30)
    set_tests_properties(agc_shader_replay_requests agc_shader_replay_tool PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_shader_replay_requests agc_shader_replay_tool PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_executable(agc_frame_replay EXCLUDE_FROM_ALL
        tools/FrameReplay.cpp
        Execution/src/FrameCapture.cpp
)
target_include_directories(agc_frame_replay PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_frame_replay PRIVATE libSceAgcDriver libkernel libc libzstd_static)
if(WIN32)
    target_link_options(agc_frame_replay PRIVATE LINKER:--image-base=0x500000000000)
endif()
set_target_properties(agc_frame_replay PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_frame_replay)

add_executable(agc_pm4_replay EXCLUDE_FROM_ALL
        tools/Pm4Replay.cpp
        tools/Pm4Analysis.cpp
)
target_include_directories(agc_pm4_replay PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_pm4_replay PRIVATE libSceAgcDriver libc)
set_target_properties(agc_pm4_replay PROPERTIES CXX_EXTENSIONS OFF)
if(MINGW)
    target_link_options(agc_pm4_replay PRIVATE -municode)
endif()
configure_windows_unwind(agc_pm4_replay)

if(MINGW)
    get_filename_component(replayCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(replayTool IN ITEMS agc_shader_replay agc_pm4_replay)
        add_custom_command(TARGET ${replayTool} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${replayCompilerDirectory}/libgcc_s_seh-1.dll"
                "${replayCompilerDirectory}/libstdc++-6.dll"
                "${replayCompilerDirectory}/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:${replayTool}>"
                VERBATIM
        )
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(agc_driver_pm4_replay_tests tests/Pm4Replay.cpp tools/Pm4Analysis.cpp)
    target_include_directories(agc_driver_pm4_replay_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_pm4_replay_tests PRIVATE libSceAgc libSceAgcDriver libkernel libc)
    set_target_properties(agc_driver_pm4_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_pm4_replay_tests)
    add_dependencies(agc_driver_pm4_replay_tests agc_pm4_replay)
    set(agcPm4ReplayCapture "${CMAKE_CURRENT_BINARY_DIR}/pm4-replay-capture")
    add_test(NAME agc_driver_pm4_replay COMMAND agc_driver_pm4_replay_tests)
    set_tests_properties(agc_driver_pm4_replay PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DUMP_SUBMITS=${agcPm4ReplayCapture}"
            FIXTURES_SETUP agc_pm4_replay_capture TIMEOUT 30)
    add_test(NAME agc_driver_pm4_replay_tool COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:agc_pm4_replay> "-DCAPTURE=${agcPm4ReplayCapture}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/Pm4ReplayTool.cmake)
    set_tests_properties(agc_driver_pm4_replay_tool PROPERTIES
            FIXTURES_REQUIRED agc_pm4_replay_capture TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_pm4_replay agc_driver_pm4_replay_tool PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    if(WIN32)
        add_test_executable(agc_driver_frame_capture_eop_tests tests/FrameCaptureEop.cpp Execution/src/FrameCapture.cpp)
        target_include_directories(agc_driver_frame_capture_eop_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR} ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR})
        target_link_libraries(agc_driver_frame_capture_eop_tests PRIVATE libSceAgcDriver libkernel libc libzstd_static)
        set_target_properties(agc_driver_frame_capture_eop_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(agc_driver_frame_capture_eop_tests)
        add_test(NAME agc_driver_frame_capture_eop COMMAND agc_driver_frame_capture_eop_tests)
        set_tests_properties(agc_driver_frame_capture_eop PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-capture-eop;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=3"
                TIMEOUT 120)
        set_tests_properties(agc_driver_frame_capture_eop PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_driver_frame_replay_state_tests tests/FrameReplayState.cpp Execution/src/FrameCapture.cpp)
    target_include_directories(agc_driver_frame_replay_state_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_frame_replay_state_tests PRIVATE libSceAgcDriver libkernel libc libzstd_static)
    set_target_properties(agc_driver_frame_replay_state_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_frame_replay_state_tests)
    set(agcFrameReplayModes order tokens blocking)
    if(WIN32)
        list(APPEND agcFrameReplayModes zero shader shader_host)
    endif()
    foreach(mode IN LISTS agcFrameReplayModes)
        set(capture "${CMAKE_CURRENT_BINARY_DIR}/frame-replay-${mode}")
        add_test(NAME agc_driver_frame_replay_${mode} COMMAND agc_driver_frame_replay_state_tests ${mode} "${capture}")
        set_tests_properties(agc_driver_frame_replay_${mode} PROPERTIES TIMEOUT 30)
        if(mode MATCHES "^shader")
            set_tests_properties(agc_driver_frame_replay_${mode} PROPERTIES ENVIRONMENT
                    "APS5_FRAME_CAPTURE=${capture};APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=1")
        endif()
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()

    if(WIN32)
        add_test_executable(agc_driver_frame_replay_tests tests/FrameReplay.cpp Execution/src/FrameCapture.cpp)
        target_include_directories(agc_driver_frame_replay_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(agc_driver_frame_replay_tests PRIVATE libSceAgc libSceAgcDriver libkernel libc libzstd_static)
        set_target_properties(agc_driver_frame_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(agc_driver_frame_replay_tests)
        add_dependencies(agc_driver_frame_replay_tests agc_frame_replay)
        add_test(NAME agc_driver_frame_replay COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay>)
        set_tests_properties(agc_driver_frame_replay PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=2"
                TIMEOUT 120)
        set_tests_properties(agc_driver_frame_replay PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_executable(agc_compute_pipeline_replay EXCLUDE_FROM_ALL tools/ComputePipelineReplay.cpp)
target_include_directories(agc_compute_pipeline_replay PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_compute_pipeline_replay PRIVATE libSceAgcDriver libc)
set_target_properties(agc_compute_pipeline_replay PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_compute_pipeline_replay)

if(BUILD_TESTING)
    add_test_executable(agc_compute_pipeline_capture_tests tests/ComputePipelineCapture.cpp Execution/src/ComputePipelineCapture.cpp)
    target_include_directories(agc_compute_pipeline_capture_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(agc_compute_pipeline_capture_tests PRIVATE libc)
    set_target_properties(agc_compute_pipeline_capture_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_compute_pipeline_capture_tests)
    add_test(NAME agc_compute_descriptor_layout_flags COMMAND agc_driver_graphics_tests --descriptor-layout-flags)
    set_tests_properties(agc_compute_descriptor_layout_flags PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_covered_centroid_state COMMAND agc_driver_graphics_tests --covered-centroid-state)
    set_tests_properties(agc_driver_covered_centroid_state PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_location_grid_fallback COMMAND agc_driver_graphics_tests --sample-location-grid-fallback)
    set_tests_properties(agc_driver_sample_location_grid_fallback PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_unobserved_provoking_vertex COMMAND agc_driver_graphics_tests --unobserved-provoking-vertex)
    set_tests_properties(agc_driver_unobserved_provoking_vertex PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_order_planning COMMAND agc_driver_graphics_tests --sample-order)
    set_tests_properties(agc_driver_sample_order_planning PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_compute_descriptor_layout_flags PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_covered_centroid_state PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_sample_location_grid_fallback PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_unobserved_provoking_vertex PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_sample_order_planning PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_compute_pipeline_capture COMMAND agc_compute_pipeline_capture_tests)
    set_tests_properties(agc_compute_pipeline_capture PROPERTIES TIMEOUT 30)
    foreach(mode IN ITEMS entry-cap byte-cap io invalid)
        string(REPLACE "-" "_" name "${mode}")
        add_test(NAME agc_compute_pipeline_capture_optional_${name} COMMAND agc_compute_pipeline_capture_tests --optional-${mode})
        set_tests_properties(agc_compute_pipeline_capture_optional_${name} PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_compute_pipeline_capture_optional_${name} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    if(WIN32)
        set_tests_properties(agc_compute_pipeline_capture PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING AND ANYPS5_ENABLE_SPIRV_TOOLS)
    add_test_executable(agc_compute_pipeline_replay_tests tests/ComputePipelineReplay.cpp)
    target_include_directories(agc_compute_pipeline_replay_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )
    target_link_libraries(agc_compute_pipeline_replay_tests PRIVATE libSceAgcDriver libc)
    set_target_properties(agc_compute_pipeline_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_compute_pipeline_replay_tests)
    add_test(NAME agc_compute_pipeline_replay COMMAND agc_compute_pipeline_replay_tests)
    set_tests_properties(agc_compute_pipeline_replay PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_dependencies(agc_compute_pipeline_replay_tests agc_compute_pipeline_replay agc_driver_branch_past_endpgm_tests)
    add_test(NAME agc_compute_pipeline_capture_run COMMAND ${CMAKE_COMMAND}
            -DFIXTURE=$<TARGET_FILE:agc_driver_branch_past_endpgm_tests>
            -DREPLAY=$<TARGET_FILE:agc_compute_pipeline_replay>
            -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/capture-tests
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ComputePipelineCaptureRun.cmake)
    set_tests_properties(agc_compute_pipeline_capture_run PROPERTIES TIMEOUT 30 SKIP_REGULAR_EXPRESSION "compute pipeline capture run: skipped, no usable Vulkan device")
    if(WIN32)
        set_tests_properties(agc_compute_pipeline_replay agc_compute_pipeline_capture_run PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(texture_format_lookup_tests ${LIBS_INCLUDE_DIR}/tests/TextureFormatLookup.cpp)
    target_include_directories(texture_format_lookup_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(texture_format_lookup_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(texture_format_lookup_tests)
    add_test(NAME texture_format_lookup COMMAND texture_format_lookup_tests)
    if(WIN32)
        set_tests_properties(texture_format_lookup PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(linear_display_buffer_tests ${LIBS_INCLUDE_DIR}/tests/LinearDisplayBuffer.cpp)
    target_include_directories(linear_display_buffer_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
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
    add_test_executable(guest_agc_driver_debug_unavailable_tests ${LIBS_INCLUDE_DIR}/tests/GuestAgcDriverDebugUnavailable.cpp)
    target_include_directories(guest_agc_driver_debug_unavailable_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_agc_driver_debug_unavailable_tests PRIVATE libSceAgcDriver libc)
    configure_windows_unwind(guest_agc_driver_debug_unavailable_tests)
    add_test(NAME guest_agc_driver_debug_unavailable COMMAND guest_agc_driver_debug_unavailable_tests)
    if(WIN32)
        set_tests_properties(guest_agc_driver_debug_unavailable PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(aperture_sources_tests ${LIBS_INCLUDE_DIR}/tests/ApertureSources.cpp)
    target_link_libraries(aperture_sources_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(aperture_sources_tests)
    add_test(NAME aperture_sources COMMAND aperture_sources_tests)
    if(WIN32)
        set_tests_properties(aperture_sources PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(ir_dump_memory_tests ${LIBS_INCLUDE_DIR}/tests/IrDumpMemory.cpp)
    target_link_libraries(ir_dump_memory_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(ir_dump_memory_tests)
    add_test(NAME ir_dump_memory COMMAND ir_dump_memory_tests)
    if(WIN32)
        set_tests_properties(ir_dump_memory PROPERTIES ENVIRONMENT_MODIFICATION
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
    add_test_executable(interpolation_reference_range_tests ${LIBS_INCLUDE_DIR}/tests/InterpolationReferenceRange.cpp)
    target_link_libraries(interpolation_reference_range_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(interpolation_reference_range_tests)
    add_test(NAME interpolation_reference_range COMMAND interpolation_reference_range_tests)
    if(WIN32)
        set_tests_properties(interpolation_reference_range PROPERTIES ENVIRONMENT_MODIFICATION
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

add_test_executable(agc_buffer_resource_vcc_tests EXCLUDE_FROM_ALL tests/BufferResourceVcc.cpp)
target_link_libraries(agc_buffer_resource_vcc_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_buffer_resource_vcc_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_buffer_resource_vcc_tests)
if(BUILD_TESTING)
    set_target_properties(agc_buffer_resource_vcc_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_buffer_resource_vcc COMMAND agc_buffer_resource_vcc_tests)
    set_tests_properties(agc_buffer_resource_vcc PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_buffer_resource_vcc PROPERTIES ENVIRONMENT_MODIFICATION
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

add_test_executable(agc_gcn_decode_tests EXCLUDE_FROM_ALL tests/GcnDecode.cpp)
target_link_libraries(agc_gcn_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_gcn_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_gcn_decode_tests)
if(BUILD_TESTING)
    set_target_properties(agc_gcn_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_gcn_decode COMMAND agc_gcn_decode_tests)
    if(WIN32)
        set_tests_properties(agc_gcn_decode PROPERTIES ENVIRONMENT_MODIFICATION
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

add_test_executable(agc_depth_range_remap_tests EXCLUDE_FROM_ALL tests/DepthRangeRemap.cpp)
target_link_libraries(agc_depth_range_remap_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_depth_range_remap_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_depth_range_remap_tests)
if(BUILD_TESTING)
    set_target_properties(agc_depth_range_remap_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_depth_range_remap COMMAND agc_depth_range_remap_tests)
    set_tests_properties(agc_depth_range_remap PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_depth_range_remap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_position_invariance_tests EXCLUDE_FROM_ALL tests/PositionInvariance.cpp)
target_link_libraries(agc_position_invariance_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_position_invariance_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_position_invariance_tests)

add_test_executable(agc_vertex_layer_export_tests EXCLUDE_FROM_ALL tests/VertexLayerExport.cpp)
target_link_libraries(agc_vertex_layer_export_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_layer_export_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_layer_export_tests)
if(BUILD_TESTING)
    set_target_properties(agc_vertex_layer_export_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vertex_layer_export COMMAND agc_vertex_layer_export_tests)
    set_tests_properties(agc_vertex_layer_export PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_vertex_layer_export PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    set_target_properties(agc_position_invariance_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_position_invariance COMMAND agc_position_invariance_tests)
    set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_vertex_barrier_tests EXCLUDE_FROM_ALL tests/VertexBarrier.cpp)
target_link_libraries(agc_vertex_barrier_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_barrier_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_barrier_tests)
if(BUILD_TESTING)
    set_target_properties(agc_vertex_barrier_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vertex_barrier COMMAND agc_vertex_barrier_tests)
    set_tests_properties(agc_vertex_barrier PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_vertex_barrier PROPERTIES ENVIRONMENT_MODIFICATION
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

add_test_executable(agc_embedded_vertex_fetch_aliasing_tests EXCLUDE_FROM_ALL tests/EmbeddedVertexFetchAliasing.cpp)
target_link_libraries(agc_embedded_vertex_fetch_aliasing_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_embedded_vertex_fetch_aliasing_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_embedded_vertex_fetch_aliasing_tests)
if(BUILD_TESTING)
    set_target_properties(agc_embedded_vertex_fetch_aliasing_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_embedded_vertex_fetch_aliasing COMMAND agc_embedded_vertex_fetch_aliasing_tests)
    if(WIN32)
        set_tests_properties(agc_embedded_vertex_fetch_aliasing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_constant_phi_tests EXCLUDE_FROM_ALL tests/ConstantPhi.cpp)
target_link_libraries(agc_constant_phi_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_constant_phi_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_constant_phi_tests)
if(BUILD_TESTING)
    set_target_properties(agc_constant_phi_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_constant_phi COMMAND agc_constant_phi_tests)
    if(WIN32)
        set_tests_properties(agc_constant_phi PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(agc_flat_scratch_tests tests/FlatScratch.cpp)
    target_link_libraries(agc_flat_scratch_tests PRIVATE shader_recompiler_agc_driver libc)
    configure_windows_unwind(agc_flat_scratch_tests)
    add_test(NAME agc_flat_scratch COMMAND agc_flat_scratch_tests)
    set_tests_properties(agc_flat_scratch PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_flat_scratch PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
add_test_executable(agc_constant_select_tests EXCLUDE_FROM_ALL tests/ConstantSelect.cpp)
target_link_libraries(agc_constant_select_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_constant_select_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_constant_select_tests)
if(BUILD_TESTING)
    set_target_properties(agc_constant_select_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_constant_select COMMAND agc_constant_select_tests)
    if(WIN32)
        set_tests_properties(agc_constant_select PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    get_directory_property(agcRegisteredTests TESTS)
    list(FILTER agcRegisteredTests EXCLUDE REGEX "^(agc_pipeline_journal(_long_paths|_extended_paths)?|agc_driver_pipeline_journal)$")
    set_property(TEST ${agcRegisteredTests} APPEND PROPERTY ENVIRONMENT "APS5_NO_PIPELINE_JOURNAL=1")
endif()
