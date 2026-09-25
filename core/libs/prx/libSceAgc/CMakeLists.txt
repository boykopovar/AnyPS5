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

add_executable(agc_command_tests EXCLUDE_FROM_ALL tests/Commands.cpp)
target_include_directories(agc_command_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_command_tests PRIVATE libSceAgc libc)
configure_windows_unwind(agc_command_tests)
configure_windows_unwind(libSceAgc)
