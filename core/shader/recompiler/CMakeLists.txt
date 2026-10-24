function(add_shader_recompiler target runtimeTarget)
    cmake_parse_arguments(PARSE_ARGV 2 shaderCache "" "" "GUARD_SOURCES")
    if(NOT TARGET "${runtimeTarget}")
        message(FATAL_ERROR "Shader recompiler requires an existing runtime target: ${runtimeTarget}")
    endif()

    if(ANYPS5_ENABLE_SPIRV_TOOLS AND NOT TARGET SPIRV-Tools-opt)
        set(SPIRV-Headers_SOURCE_DIR "${ANYPS5_SPIRV_HEADERS_SOURCE_DIR}")
        set(SPIRV_SKIP_EXECUTABLES ON)
        set(SPIRV_SKIP_TESTS ON)
        set(SPIRV_TOOLS_BUILD_STATIC ON)
        set(SPIRV_WERROR OFF)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory("${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Tools" "${CMAKE_BINARY_DIR}/3rdparty/SPIRV-Tools" EXCLUDE_FROM_ALL)
        include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PatchSpirvTools.cmake")
    endif()

    if(NOT TARGET glslang)
        set(BUILD_SHARED_LIBS OFF)
        set(BUILD_EXTERNAL OFF)
        set(GLSLANG_TESTS OFF)
        set(GLSLANG_ENABLE_INSTALL OFF)
        set(ENABLE_GLSLANG_BINARIES OFF)
        set(ENABLE_SPIRV ON)
        set(ENABLE_SPVREMAPPER OFF)
        set(ENABLE_OPT OFF)
        set(ALLOW_EXTERNAL_SPIRV_TOOLS OFF)

        add_subdirectory(
                "${PROJECT_SOURCE_DIR}/3rdparty/glslang"
                "${CMAKE_BINARY_DIR}/3rdparty/glslang"
                EXCLUDE_FROM_ALL
        )
    endif()

    add_library(${target} STATIC EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Recompiler.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StencilReplay.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderDiskCache.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderCachePack.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/RectListShaders.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/BarycentricGeometryShader.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaOpcode.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaProgram.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaInstructionDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/GcnInstructionDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaScalarOpDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaVectorOpDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaImageOpDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaExportOpDecoder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaDescriptorFormat.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ControlFlow/src/ControlFlowGraph.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ControlFlow/src/GraphBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ControlFlow/src/Structurizer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ControlFlow/src/RequestSerializer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrType.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/GuestRegister.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrOpcode.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrValue.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBlock.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrProgram.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderInternal.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderCore.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderConstants.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderRegisters.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderControlFlow.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderExtendedRegisters.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderConversions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderArithmetic.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/src/IrBuilder/IrBuilderLogic.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/EmbeddedVertexFetch.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/InstructionTranslator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/ShaderInputInfoBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/TranslationContext/OperandAccess.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/TranslationContext/SubwordPacking.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/TranslationContext/ExecMask.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/ScalarInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/RoundingMode.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/VectorInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/FloatInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/Float64Instructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/IntegerInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/CompareInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/ControlFlowInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/ConvertInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/MemoryResourceHelpers.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/BufferInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/FlatInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/ImageInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/MemoryInstructions/MemoryInstructionDispatch.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/AttributeInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/src/DispatchInstructions.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaVariable.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaDefTable.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaPass.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaInstructionVisitor.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SsaBuilder/SsaRegisterStateCleanup.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ConstantFolder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/DeadCodeEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/UnobservedPhiEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/HostInterpolationChecker.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ReadLaneEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/MaskedSelectEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/TessellationLowering.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ResourceTracker.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ResourceMaterializer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/BindingAllocator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/RequestMemoryView.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/DescriptorBindingBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ShaderInfoCollector.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SharedMemoryBarrierInserter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/DenormalFlushEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/VectorWindowStoreEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/RedundantValueEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtAddressArithmetic.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtDiagnostics.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtEvaluator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtFlatSlotClasses.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtInstructionPredicates.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtPlanBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtRuntimeValidator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtTape.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtWalker.cpp

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvModule.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAnalysis.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterMath.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterFloat64.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterReciprocal.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterRegisters.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvFlowEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvImageEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvModuleEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaLookup.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaFault.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaRead.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBufferFormat.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvRuntimeBuffer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvSpecialization.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvSpannedUniformity.cpp

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvBufferAccess.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvConstants.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvDescriptors.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvFormatConvert.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvInputOutput.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvModuleSetup.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvSubgroup.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvTypes.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvEmitterHelpers.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvMemory/SpirvMemoryInstructions.cpp

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/DummyShaders.cpp
    )

    target_include_directories(${target} PUBLIC
            ${PROJECT_SOURCE_DIR}/core/libs
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/include
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ControlFlow/include
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/IntermediateRepresentation/include
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Translation/include
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/include
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/include
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
            ${ANYPS5_GLSLANG_INCLUDE_DIRS}
    )

    target_compile_features(${target} PUBLIC cxx_std_20)
    if(WIN32)
        target_compile_options(${target} PRIVATE "SHELL:-include ${PROJECT_SOURCE_DIR}/core/libs/prx/libc/include/HostStandardStreams.hpp")
    endif()

    if(ANYPS5_ENABLE_SPIRV_TOOLS)
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvOptimizer.cpp)
    endif()

    set(shaderCacheGeneratedDir "${CMAKE_CURRENT_BINARY_DIR}/${target}_generated")
    set(shaderCacheVersionSource "${shaderCacheGeneratedDir}/ShaderCacheVersion.cpp")
    file(GLOB_RECURSE shaderCacheRecompilerSources CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.hpp"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.h"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.cmake"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CMakeLists.txt"
    )
    get_target_property(shaderCacheCompiledSources ${target} SOURCES)
    list(APPEND shaderCacheRecompilerSources ${shaderCacheCompiledSources})
    list(REMOVE_DUPLICATES shaderCacheRecompilerSources)
    set(shaderCacheKeySources
            "${PROJECT_SOURCE_DIR}/core/libs/prx/libc/include/HostThreadLocal.hpp"
            "${PROJECT_SOURCE_DIR}/core/libs/prx/libc/include/general/AtomicSharedPtr.hpp")
    foreach(source IN LISTS shaderCacheKeySources)
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "Shader disk cache key source does not exist: ${source}")
        endif()
        list(APPEND shaderCacheRecompilerSources "${source}")
    endforeach()
    set(shaderCacheGuardSources)
    foreach(source IN LISTS shaderCache_GUARD_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND shaderCacheGuardSources "${source}")
    endforeach()
    list(REMOVE_ITEM shaderCacheGuardSources ${shaderCacheRecompilerSources})

    find_package(Git QUIET)
    set(shaderCacheSubmoduleStamps)
    if(NOT ANYPS5_USE_SYSTEM_DEPS)
        set(shaderCacheSubmodules "${ANYPS5_SPIRV_HEADERS_SOURCE_DIR}" "${ANYPS5_GLSLANG_SOURCE_DIR}")
        if(ANYPS5_ENABLE_SPIRV_TOOLS)
            list(APPEND shaderCacheSubmodules "${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Tools")
        endif()
        foreach(submodule IN LISTS shaderCacheSubmodules)
            list(APPEND shaderCacheSubmoduleStamps "${submodule}/CMakeLists.txt")
            if(NOT GIT_FOUND)
                continue()
            endif()
            foreach(ref IN ITEMS HEAD refs/heads)
                execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" rev-parse --path-format=absolute --git-path ${ref}
                        OUTPUT_VARIABLE stamp OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE status ERROR_QUIET)
                if(status EQUAL 0 AND EXISTS "${stamp}" AND NOT IS_DIRECTORY "${stamp}")
                    list(APPEND shaderCacheSubmoduleStamps "${stamp}")
                endif()
            endforeach()
        endforeach()
    endif()

    get_target_property(shaderCacheIncludeDirectories ${target} INCLUDE_DIRECTORIES)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" shaderCacheBuildType)
    set(shaderCacheExtra "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_BUILD_TYPE} spirv-tools=${ANYPS5_ENABLE_SPIRV_TOOLS} ${CMAKE_SYSTEM_PROCESSOR} flags=${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${shaderCacheBuildType}}")
    if(ANYPS5_USE_SYSTEM_DEPS)
        string(APPEND shaderCacheExtra " system-deps=${ANYPS5_SYSTEM_DEPS_STAMP}")
    endif()
    set(shaderCacheReviewedNames
            BdaAbi::RangesOrdered
            BuildBarycentricGeometryShader
            BuildBarycentricTessellationShaders
            BuildPreparedShaderKey
            BuildRectListShaders
            CaptureResources
            DebugProbeActive
            DepthBitsTextureWidth
            HashBytes
            LayoutBarycentricEmulation
            MeshBarycentricLayoutOf
            NativeCachePath_nid_no_patch
            ImageTableAbi::PoisonCode
            ImageTableAbi::PoisonReasonName
            ImageTableAbi::TableHeader
            FmaskLayoutOf
            IsFmaskTextureFormat
            PipelineSpecialization::IsWorkgroupOffset
            PipelineSpecialization::WorkgroupOffsetBase
            PipelineSpecialization::WorkgroupOffsetWords
            PixelInput
            PixelInputBit
            RayTracingStrict
            RayTracingMiss
            ReadWholeFile
            Recompile
            RecompileCacheKey::ContextHash
            RecompileResult
            PrepareShader
            GetPreparedCode
            PreparedShaderInvocation::TryCreate
            MatchesPreparedShader
            GetPreparedArtifact
            MaterializeShader
            RegistrationResult
            ResolveSource
            ResourceMaterializer::EvaluateNanoseconds
            ResourceMaterializer::MaterializeNanoseconds
            ReleaseUndrawnResourcePlan
            ResourceMaterializer::SpecializationNanoseconds
            RuntimeAbi::RequireVersion
            GcnGuestShaders
            SetGcnGuestShaders
            GcnImageTilingIndex
            GcnImagePitch
            GcnImagePow2Pad
            RuntimeAbi::HeapCapacity
            SampledTextureNumericClass
            SetDebugProbeActive
            ShaderCacheDirectory
            ShaderDiskCache::Shutdown
            ShaderStage
            HashSpirvWords
            ShaderDiskCache::DecodeResult
            ShaderDiskCache::Enabled
            ShaderDiskCache::EncodeResult
            ShaderDiskCache::EntryDirectory
            ShaderDiskCache::SourceVersion
            SrgbDecodeBit
            SrgbUnormFormat
            FindUserDataTail
            LinkUserDataTail
            ThreadOwned
            ProgramRole
            UnresolvableSwappcTarget
            ValidateAndOptimizeSpirv
            FindUserDataCalls
            UserDataCalleeLength
            LinkUserDataCalls
            FindTableCalls
            LinkCalls
            LoadStamp
            WriteFileAtomically
    )
    set(shaderCacheManifest)
    foreach(section IN ITEMS sources guard submodules includes reviewed)
        if(section STREQUAL "sources")
            set(entries ${shaderCacheRecompilerSources})
        elseif(section STREQUAL "guard")
            set(entries ${shaderCacheGuardSources})
        elseif(section STREQUAL "submodules")
            set(entries ${shaderCacheSubmodules})
        elseif(section STREQUAL "includes")
            set(entries ${shaderCacheIncludeDirectories})
        else()
            set(entries ${shaderCacheReviewedNames})
        endif()
        foreach(entry IN LISTS entries)
            string(APPEND shaderCacheManifest "${section} ${entry}\n")
        endforeach()
    endforeach()
    set(shaderCacheListFile "${shaderCacheGeneratedDir}/sources.txt")
    file(CONFIGURE OUTPUT "${shaderCacheListFile}" CONTENT "${shaderCacheManifest}" @ONLY)
    add_custom_command(
            OUTPUT "${shaderCacheVersionSource}"
            COMMAND ${CMAKE_COMMAND}
            "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DLIST_FILE=${shaderCacheListFile}"
            "-DRECOMPILER_DIR=${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
            "-DGIT=${GIT_EXECUTABLE}"
            "-DEXTRA=${shaderCacheExtra}"
            "-DOUTPUT=${shaderCacheVersionSource}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderCacheVersion.cmake"
            DEPENDS ${shaderCacheRecompilerSources} ${shaderCacheGuardSources} ${shaderCacheSubmoduleStamps} "${shaderCacheListFile}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderCacheVersion.cmake"
            COMMENT "Hashing the shader recompiler sources for the shader disk cache"
            VERBATIM
    )
    target_sources(${target} PRIVATE "${shaderCacheVersionSource}")

    set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            CXX_EXTENSIONS OFF
    )

    target_link_libraries(${target} PRIVATE
            ${runtimeTarget}
            glslang
            SPIRV
    )

    if(ANYPS5_ENABLE_SPIRV_TOOLS)
        target_link_libraries(${target} PRIVATE SPIRV-Tools-opt SPIRV-Tools-static)
    endif()
    target_compile_definitions(${target} PUBLIC ANYPS5_ENABLE_SPIRV_TOOLS=$<BOOL:${ANYPS5_ENABLE_SPIRV_TOOLS}>)
    if(BUILD_TESTING AND NOT TARGET spanned_uniformity_tests)
        add_test_executable(spanned_uniformity_tests
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SpannedUniformityTests.cpp"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvSpannedUniformity.cpp")
        target_include_directories(spanned_uniformity_tests PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/include"
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR})
        set_target_properties(spanned_uniformity_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(spanned_uniformity_tests)
        add_test(NAME spanned_uniformity COMMAND spanned_uniformity_tests)
        set_tests_properties(spanned_uniformity PROPERTIES TIMEOUT 20)
        if(MINGW)
            target_link_libraries(spanned_uniformity_tests PRIVATE libc)
            set_tests_properties(spanned_uniformity PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
    if(BUILD_TESTING AND ANYPS5_ENABLE_SPIRV_TOOLS AND NOT ANYPS5_USE_SYSTEM_DEPS AND NOT TARGET spirv_validator_exit_tests)
        add_test_executable(spirv_validator_exit_tests "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SpirvValidatorExit.cpp")
        target_link_libraries(spirv_validator_exit_tests PRIVATE SPIRV-Tools-static)
        set_target_properties(spirv_validator_exit_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(spirv_validator_exit_tests)
        add_test(NAME spirv_validator_exit COMMAND spirv_validator_exit_tests)
        set_tests_properties(spirv_validator_exit PROPERTIES TIMEOUT 30)
        if(MINGW)
            target_link_libraries(spirv_validator_exit_tests PRIVATE libc)
            set_tests_properties(spirv_validator_exit PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
endfunction()
