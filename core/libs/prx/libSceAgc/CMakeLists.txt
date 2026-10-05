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

add_test_executable(agc_command_tests EXCLUDE_FROM_ALL tests/Commands.cpp)
target_include_directories(agc_command_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_command_tests PRIVATE libSceAgc libc)
configure_windows_unwind(agc_command_tests)
configure_windows_unwind(libSceAgc)

add_test_executable(agc_platform_tests EXCLUDE_FROM_ALL tests/Platform.cpp)
target_include_directories(agc_platform_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_platform_tests PRIVATE libSceAgc libc)
configure_windows_unwind(agc_platform_tests)
if(BUILD_TESTING)
    set_target_properties(agc_platform_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME agc_platform_tests COMMAND agc_platform_tests)
    if(WIN32)
        set_tests_properties(agc_platform_tests PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_register_command_tests EXCLUDE_FROM_ALL
        tests/RegisterCommands.cpp
        Command/src/Packet.cpp
        Cb/src/Registers.cpp
        DcbState/src/Registers.cpp
        Patch/src/IndirectRegisters.cpp
)
target_include_directories(agc_register_command_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_register_command_tests PRIVATE libc)
configure_windows_unwind(agc_register_command_tests)
if(BUILD_TESTING)
    set_target_properties(agc_register_command_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME agc_register_command_tests COMMAND agc_register_command_tests)
    if(WIN32)
        set_tests_properties(agc_register_command_tests PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_branch_patch_tests EXCLUDE_FROM_ALL
        tests/BranchPatch.cpp
        Command/src/Packet.cpp
        Cb/src/Branch.cpp
        Patch/src/Branch.cpp
)
target_include_directories(agc_branch_patch_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_branch_patch_tests PRIVATE libc)
configure_windows_unwind(agc_branch_patch_tests)
if(BUILD_TESTING)
    set_target_properties(agc_branch_patch_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME agc_branch_patch_tests COMMAND agc_branch_patch_tests)
    if(WIN32)
        set_tests_properties(agc_branch_patch_tests PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_shader_fusion_tests EXCLUDE_FROM_ALL tests/ShaderFusion.cpp)
target_include_directories(agc_shader_fusion_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_shader_fusion_tests PRIVATE libSceAgc libc)
set_target_properties(agc_shader_fusion_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_shader_fusion_tests)

if(BUILD_TESTING)
    set_target_properties(agc_shader_fusion_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_shader_fusion COMMAND agc_shader_fusion_tests)
    if(WIN32)
        set_tests_properties(agc_shader_fusion PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_interpolant_mapping_tests EXCLUDE_FROM_ALL tests/InterpolantMapping.cpp)
target_include_directories(agc_interpolant_mapping_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_interpolant_mapping_tests PRIVATE libSceAgc libc)
set_target_properties(agc_interpolant_mapping_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_interpolant_mapping_tests)

if(BUILD_TESTING)
    set_target_properties(agc_interpolant_mapping_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_interpolant_mapping COMMAND agc_interpolant_mapping_tests)
    if(WIN32)
        set_tests_properties(agc_interpolant_mapping PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_shader_link_tests ${LIBS_INCLUDE_DIR}/tests/GuestShaderLink.cpp)
    target_include_directories(guest_shader_link_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_shader_link_tests PRIVATE libSceAgc libc)
    configure_windows_unwind(guest_shader_link_tests)
    add_test(NAME guest_shader_link COMMAND guest_shader_link_tests)
    if(WIN32)
        set_tests_properties(guest_shader_link PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_dma_data_tests EXCLUDE_FROM_ALL
        tests/DmaData.cpp
        Command/src/Packet.cpp
        Command/src/Memory.cpp
        Acb/src/Memory.cpp
        DcbFlow/src/Memory.cpp
)
target_include_directories(agc_dma_data_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_dma_data_tests PRIVATE libc)
configure_windows_unwind(agc_dma_data_tests)
if(BUILD_TESTING)
    set_target_properties(agc_dma_data_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME agc_dma_data_tests COMMAND agc_dma_data_tests)
    if(WIN32)
        set_tests_properties(agc_dma_data_tests PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(agc_prime_utcl2_tests EXCLUDE_FROM_ALL
        tests/PrimeUtcl2.cpp
        Command/src/Packet.cpp
        Command/src/Memory.cpp
        Acb/src/Memory.cpp
        DcbFlow/src/Memory.cpp
        DcbState/src/Display.cpp
)
target_include_directories(agc_prime_utcl2_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_prime_utcl2_tests PRIVATE libc)
configure_windows_unwind(agc_prime_utcl2_tests)
if(BUILD_TESTING)
    set_target_properties(agc_prime_utcl2_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME agc_prime_utcl2_tests COMMAND agc_prime_utcl2_tests)
    if(WIN32)
        set_tests_properties(agc_prime_utcl2_tests PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
