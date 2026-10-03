function(add_shader_recompiler target runtimeTarget)
    if(NOT TARGET "${runtimeTarget}")
        message(FATAL_ERROR "Shader recompiler requires an existing runtime target: ${runtimeTarget}")
    endif()

    if(ANYPS5_ENABLE_SPIRV_TOOLS AND NOT TARGET SPIRV-Tools-opt)
        set(SPIRV-Headers_SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Headers")
        set(SPIRV_SKIP_EXECUTABLES ON)
        set(SPIRV_SKIP_TESTS ON)
        set(SPIRV_TOOLS_BUILD_STATIC ON)
        set(SPIRV_WERROR OFF)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory("${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Tools" "${CMAKE_BINARY_DIR}/3rdparty/SPIRV-Tools" EXCLUDE_FROM_ALL)
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
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderDiskCache.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/RectListShaders.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaOpcode.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaProgram.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RdnaDecoder/src/RdnaInstructionDecoder.cpp
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
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ReadLaneEliminator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ResourceTracker.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ResourceMaterializer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/BindingAllocator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/RequestMemoryView.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/DescriptorBindingBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/ShaderInfoCollector.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SharedMemoryBarrierInserter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtAddressArithmetic.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtDescriptorEvaluation.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtDiagnostics.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtEvaluator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtFlatSlotClasses.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtInstructionPredicates.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtPlanBuilder.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtRuntimeValidator.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Optimization/src/SrtWalker/SrtWalker.cpp

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvModule.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAnalysis.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterMath.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterFloat64.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvAlu/SpirvAluEmitterRegisters.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvFlowEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvImageEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvModuleEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaLookup.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaFault.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBdaRead.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvEmitter.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvBufferFormat.cpp

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
            ${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
            ${PROJECT_SOURCE_DIR}/3rdparty/glslang
    )

    target_compile_features(${target} PUBLIC cxx_std_20)

    set(shaderCacheGeneratedDir "${CMAKE_CURRENT_BINARY_DIR}/${target}_generated")
    set(shaderCacheVersionHeader "${shaderCacheGeneratedDir}/ShaderCacheVersion.hpp")
    set(shaderCacheDriverDir "${PROJECT_SOURCE_DIR}/core/libs/prx/libSceAgcDriver")
    file(GLOB_RECURSE shaderCacheSources CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.cpp"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.hpp"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.h"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.cmake"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CMakeLists.txt"
            "${shaderCacheDriverDir}/*.cpp"
            "${shaderCacheDriverDir}/*.hpp"
            "${shaderCacheDriverDir}/*.h"
            "${shaderCacheDriverDir}/*.comp"
            "${shaderCacheDriverDir}/*.cmake"
            "${shaderCacheDriverDir}/CMakeLists.txt"
    )
    list(FILTER shaderCacheSources EXCLUDE REGEX "^${shaderCacheDriverDir}/(tests|tools)/")
    if(ANYPS5_ENABLE_SPIRV_TOOLS AND EXISTS "${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Tools/CHANGES")
        list(APPEND shaderCacheSources "${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Tools/CHANGES")
    endif()
    list(JOIN shaderCacheSources "\n" shaderCacheSourceList)
    set(shaderCacheListFile "${shaderCacheGeneratedDir}/sources.txt")
    file(CONFIGURE OUTPUT "${shaderCacheListFile}" CONTENT "${shaderCacheSourceList}\n" @ONLY)
    set(shaderCacheExtra "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_BUILD_TYPE} spirv-tools=${ANYPS5_ENABLE_SPIRV_TOOLS} ${CMAKE_SYSTEM_PROCESSOR}")
    add_custom_command(
            OUTPUT "${shaderCacheVersionHeader}"
            COMMAND ${CMAKE_COMMAND}
            "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DLIST_FILE=${shaderCacheListFile}"
            "-DRECOMPILER_DIR=${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
            "-DEXTRA=${shaderCacheExtra}"
            "-DOUTPUT=${shaderCacheVersionHeader}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderCacheVersion.cmake"
            DEPENDS ${shaderCacheSources} "${shaderCacheListFile}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderCacheVersion.cmake"
            COMMENT "Hashing the shader recompiler and AGC driver sources for the shader disk cache"
            VERBATIM
    )
    target_sources(${target} PRIVATE "${shaderCacheVersionHeader}")
    target_include_directories(${target} PRIVATE "${shaderCacheGeneratedDir}")

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
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SpirvBackend/src/SpirvOptimizer.cpp)
        target_link_libraries(${target} PRIVATE SPIRV-Tools-opt SPIRV-Tools-static)
    endif()
    target_compile_definitions(${target} PUBLIC ANYPS5_ENABLE_SPIRV_TOOLS=$<BOOL:${ANYPS5_ENABLE_SPIRV_TOOLS}>)
endfunction()
