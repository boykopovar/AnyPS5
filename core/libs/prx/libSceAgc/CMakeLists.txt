add_library(libSceAgc SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Branch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Registers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/RegisterDefaults.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/Packet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/src/DrawIndexed.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/src/DrawNonIndexed.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/src/IndexBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/src/Instancing.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/src/Stats.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbFlow/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbFlow/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbFlow/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbFlow/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/ContextState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Display.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Init.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Marker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Predication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Registers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/src/Workload.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/src/PacketInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/src/Platform.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/src/RegisterDefaults.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/src/ShaderFusion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/src/Suspend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/CondExec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/Dma.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/IndirectRegisters.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/Jump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/Nop.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/QueueEndOfPipeAction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/Rewind.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/WaitRegMem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/src/Branch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Predication/src/Predication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/CreateShader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/GsOversubscription.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/InterpolantMapping.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/LinkShaders.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/PrimState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/ShaderUtils.cpp
)

target_include_directories(libSceAgc PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAgc PRIVATE libSceAgcDriver libc)

option(APS5_AGC_CREATE_LOG "Logging sceAgcCreateShader Ok status" ON)

target_compile_definitions(libSceAgc PRIVATE
        APS5_AGC_CREATE_LOG=$<BOOL:${APS5_AGC_CREATE_LOG}>
)

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()

configure_windows_unwind(libSceAgc)

if(BUILD_TESTING)
    set(agcTestDir ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set(agcSourceDir ${CMAKE_CURRENT_SOURCE_DIR})

    anyps5_add_library_test(agc_command_tests
        CUSTOM_MAIN
        NAME agc_commands
        SOURCES ${agcTestDir}/integration/Commands.cpp
        LINK libSceAgc libc
        RUNTIME libc
    )
    add_test(NAME agc_initialization COMMAND agc_command_tests --case Init_VersionsAndRegisterDefaults_AcceptSupportedAndRejectOthers)
    anyps5_set_test_properties(agc_initialization integration "" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_initialization PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    foreach(agcTest IN ITEMS platform:Platform packet_info:PacketInfo payload_range:PayloadRange
            end_of_shader_action_size:EndOfShaderActionSize context_state_size:ContextStateSize)
        string(REPLACE ":" ";" agcTest "${agcTest}")
        list(GET agcTest 0 agcTestName)
        list(GET agcTest 1 agcTestSource)
        anyps5_add_library_test(agc_${agcTestName}_tests
            CUSTOM_MAIN
            NAME agc_${agcTestName}_tests
            SOURCES ${agcTestDir}/integration/${agcTestSource}.cpp
            LINK libSceAgc libc
            RUNTIME libc
        )
    endforeach()

    anyps5_add_library_test(agc_register_command_tests
        LABEL unit
        NAME agc_register_command_tests
        SOURCES
            ${agcTestDir}/unit/RegisterCommands.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Cb/src/Registers.cpp
            ${agcSourceDir}/DcbState/src/Registers.cpp
            ${agcSourceDir}/Patch/src/IndirectRegisters.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_cb_cond_write_tests
        LABEL unit
        NAME agc_cb_cond_write_tests
        SOURCES
            ${agcTestDir}/unit/CbCondWrite.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Command/src/Memory.cpp
            ${agcSourceDir}/Command/src/Control.cpp
            ${agcSourceDir}/Cb/src/Branch.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_branch_patch_tests
        LABEL unit
        NAME agc_branch_patch_tests
        SOURCES
            ${agcTestDir}/unit/BranchPatch.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Cb/src/Branch.cpp
            ${agcSourceDir}/Patch/src/Branch.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_predication_tests
        LABEL unit
        NAME agc_predication_tests
        SOURCES
            ${agcTestDir}/unit/Predication.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Command/src/Control.cpp
            ${agcSourceDir}/DcbState/src/Predication.cpp
            ${agcSourceDir}/DcbDraw/src/IndexBuffer.cpp
            ${agcSourceDir}/Predication/src/Predication.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_shader_fusion_tests
        CUSTOM_MAIN
        LABEL gpu
        SOURCES ${agcTestDir}/gpu/ShaderFusion.cpp
        LINK libSceAgc libSceAgcDriver libc
        RUNTIME libc
        INCLUDES
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
            ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
    )
    set_target_properties(agc_shader_fusion_tests PROPERTIES CXX_EXTENSIONS OFF)

    anyps5_add_library_test(agc_interpolant_mapping_tests
        CUSTOM_MAIN
        LABEL unit
        SOURCES
            ${agcTestDir}/unit/InterpolantMapping.cpp
            ${agcSourceDir}/Shader/src/InterpolantMapping.cpp
            ${agcSourceDir}/Shader/src/ShaderUtils.cpp
        LINK libSceAgcDriver libc
        RUNTIME libc
    )
    set_target_properties(agc_interpolant_mapping_tests PROPERTIES CXX_EXTENSIONS OFF)

    anyps5_add_library_test(guest_shader_link_tests
        SOURCES
            ${LIBS_INCLUDE_DIR}/tests/integration/GuestShaderLink.cpp
            ${agcSourceDir}/Shader/src/LinkShaders.cpp
            ${agcSourceDir}/Shader/src/PrimState.cpp
            ${agcSourceDir}/Shader/src/InterpolantMapping.cpp
            ${agcSourceDir}/Shader/src/ShaderUtils.cpp
        LINK libSceAgc libSceAgcDriver libc
        RUNTIME libc
    )

    anyps5_add_library_test(agc_gs_oversubscription_tests
        CUSTOM_MAIN
        LABEL unit
        NAME agc_gs_oversubscription_tests
        SOURCES
            ${agcTestDir}/unit/GsOversubscription.cpp
            ${agcSourceDir}/Shader/src/GsOversubscription.cpp
        LINK libc
    )

    foreach(agcTest IN ITEMS dma_data:DmaData write_data_patch:WriteDataPatch prime_utcl2:PrimeUtcl2)
        string(REPLACE ":" ";" agcTest "${agcTest}")
        list(GET agcTest 0 agcTestName)
        list(GET agcTest 1 agcTestSource)
        set(agcTestSources
            ${agcTestDir}/unit/${agcTestSource}.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Command/src/Memory.cpp
            ${agcSourceDir}/Acb/src/Memory.cpp
            ${agcSourceDir}/DcbFlow/src/Memory.cpp)
        if(agcTestSource STREQUAL "WriteDataPatch")
            list(APPEND agcTestSources ${agcSourceDir}/Patch/src/WaitRegMem.cpp)
        elseif(agcTestSource STREQUAL "PrimeUtcl2")
            list(APPEND agcTestSources ${agcSourceDir}/DcbState/src/Display.cpp)
        endif()
        anyps5_add_library_test(agc_${agcTestName}_tests
            LABEL unit
            NAME agc_${agcTestName}_tests
            SOURCES ${agcTestSources}
            LINK libc
        )
    endforeach()

    anyps5_add_library_test(agc_cond_exec_patch_tests
        LABEL unit
        NAME agc_cond_exec_patch_tests
        SOURCES
            ${agcTestDir}/unit/CondExecPatch.cpp
            ${agcSourceDir}/Command/src/Packet.cpp
            ${agcSourceDir}/Command/src/Memory.cpp
            ${agcSourceDir}/Command/src/Control.cpp
            ${agcSourceDir}/DcbFlow/src/Sync.cpp
            ${agcSourceDir}/Acb/src/Sync.cpp
            ${agcSourceDir}/Patch/src/CondExec.cpp
        LINK libc
    )

    anyps5_add_library_test(agc_shader_helpers_tests
        LABEL unit
        SOURCES
            ${agcTestDir}/unit/ShaderHelpers.cpp
            ${agcSourceDir}/Shader/src/LinkShaders.cpp
            ${agcSourceDir}/Shader/src/PrimState.cpp
            ${agcSourceDir}/Shader/src/InterpolantMapping.cpp
            ${agcSourceDir}/Shader/src/ShaderUtils.cpp
        LINK libc libSceAgcDriver
    )
endif()
