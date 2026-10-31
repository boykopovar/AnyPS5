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
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Memory/WrittenBufferIndex.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/HostReport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/HostReportSources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/HostReportVulkan.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/KeyboardOverlay.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/PadStatusOverlay.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/RuntimeSwitches.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/Resources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/src/DeferredDestruction.cpp
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

set_target_properties(libSceAgcDriver PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceAgcDriver)
if(WIN32)
    target_link_options(libSceAgcDriver PRIVATE LINKER:--exclude-symbols=_Znwy:_Znay:_ZnwyRKSt9nothrow_t:_ZnayRKSt9nothrow_t:_ZdlPv:_ZdaPv:_ZdlPvy:_ZdaPvy:_ZdlPvRKSt9nothrow_t:_ZdaPvRKSt9nothrow_t)
endif()

if(WIN32)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/GuestMemory.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Execution/src/Driver/Lifecycle.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0602)
endif()

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()

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

set(ANYPS5_AGC_EXECUTION_TESTS "" CACHE STRING "tests/execution sources to build, by file name without extension; the others build a stub that reports a skip. Empty builds all")

file(GLOB agcExecutionTestSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/*.cpp)
set(agcExecutionTests)
foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)
    get_filename_component(agcExecutionTestName ${agcExecutionTestSource} NAME_WE)
    if(NOT WIN32 AND agcExecutionTestName STREQUAL "CommittedReadMapping")
        continue()
    endif()
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
    if(agcExecutionTests)
        list(GET agcExecutionTests 0 agcExecutionPchTarget)
        target_precompile_headers(${agcExecutionTest} REUSE_FROM ${agcExecutionPchTarget})
    else()
        target_precompile_headers(${agcExecutionTest} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/VulkanTestDevice.hpp
                <array> <iostream> <span> <string> <vector>)
    endif()
    if(CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache")
        set_target_properties(${agcExecutionTest} PROPERTIES CXX_COMPILER_LAUNCHER
                "${CMAKE_COMMAND};-E;env;CCACHE_SLOPPINESS=pch_defines,time_macros;${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()
    list(APPEND agcExecutionTests ${agcExecutionTest})
endforeach()

function(agc_add_default_test agcTest)
    string(REGEX REPLACE "_tests$" "" agcTestName "${agcTest}")
    set_target_properties(${agcTest} PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME ${agcTestName} COMMAND ${agcTest})
    set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(${agcTestName} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endfunction()

function(agc_add_draw_pipeline_test agcTestName)
    add_test(NAME ${agcTestName}_draw_pipeline COMMAND ${agcTestName}_tests)
    set_tests_properties(${agcTestName}_draw_pipeline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(${agcTestName}_draw_pipeline PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endfunction()

function(agc_add_spirv_mode_tests agcTest)
    if(NOT TARGET ${agcTest})
        return()
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
endfunction()

if(BUILD_TESTING)
    foreach(agcTest IN LISTS agcExecutionTests)
        if(agcTest STREQUAL "agc_driver_constant_store_tests" OR agcTest STREQUAL "agc_driver_flat_apertures_tests" OR agcTest STREQUAL "agc_driver_pipeline_optimized_readback_tests" OR agcTest STREQUAL "agc_driver_rcp_hardware_tests")
            continue()
        endif()
        agc_add_default_test(${agcTest})
    endforeach()
endif()

foreach(agcExecutionTestName IN LISTS ANYPS5_AGC_EXECUTION_TESTS)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/${agcExecutionTestName}.cpp)
        message(FATAL_ERROR "ANYPS5_AGC_EXECUTION_TESTS names ${agcExecutionTestName}, which has no source in tests/execution")
    endif()
endforeach()
option(ANYPS5_REQUIRE_CPU_LEAD_TEST "Register agc_driver_sparse_dma_buf_cpu_lead, which fails instead of skipping when the nonzero-lead dma-buf CPU publish cannot run (bench only)" OFF)

file(GLOB agcTestFragments CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/*.cmake)
list(SORT agcTestFragments)
foreach(agcTestFragment IN LISTS agcTestFragments)
    include(${agcTestFragment})
endforeach()

if(BUILD_TESTING)
    set_tests_properties(agc_driver_msaa_dcc_state ${multisampledDccClearTests} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(${multisampledDccClearTests} PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_dithered_alpha_to_coverage agc_driver_two_sample_dithered_alpha_to_coverage ${multisampledDccClearTests} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
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

    set_tests_properties(${agcGuestMemoryLifetimeTests} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000;APS5_HEAP_CACHE_MIB=4"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_guest_memory_lifetime_heap_uncached PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000;APS5_HEAP_CACHE_MIB=0")
    if(WIN32)
        set_tests_properties(${agcGuestMemoryLifetimeTests} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND Python3_Interpreter_FOUND AND ANYPS5_RELINKER_CLI)
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
    endif()

    foreach(partialStoreTest agc_driver_depth_plane_partial_store_unimported agc_driver_depth_plane_partial_store_buffer_copy)
        set_tests_properties(${partialStoreTest} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_HOST_IMPORT=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(${partialStoreTest} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
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

if(BUILD_TESTING AND ANYPS5_ENABLE_SPIRV_TOOLS)
    add_test(NAME agc_compute_pipeline_inspection COMMAND ${CMAKE_COMMAND}
            -DREPLAY=$<TARGET_FILE:agc_compute_pipeline_replay>
            -DVULKAN_PROBE=$<TARGET_FILE:agc_compute_pipeline_replay_tests>
            -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/capture-tests
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ComputePipelineInspectRun.cmake)
    set_tests_properties(agc_compute_pipeline_inspection PROPERTIES TIMEOUT 60 SKIP_REGULAR_EXPRESSION "compute pipeline inspection: skipped")
    if(WIN32)
        set_tests_properties(agc_compute_pipeline_replay agc_compute_pipeline_capture_run agc_compute_pipeline_inspection PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    get_directory_property(agcRegisteredTests TESTS)
    list(FILTER agcRegisteredTests EXCLUDE REGEX "^(agc_pipeline_journal(_long_paths|_extended_paths)?|agc_driver_pipeline_journal|agc_pipeline_store_replay_growth)$")
    set_property(TEST ${agcRegisteredTests} APPEND PROPERTY ENVIRONMENT "APS5_NO_PIPELINE_JOURNAL=1")
endif()
