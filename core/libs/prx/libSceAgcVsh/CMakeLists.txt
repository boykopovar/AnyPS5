add_library(libSceAgcVsh SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Acb/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Acb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Acb/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Acb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Cb/src/Branch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Cb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Cb/src/Registers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Cb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Command/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Command/src/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Command/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Command/src/Packet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Command/src/RegisterDefaults.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbDraw/src/DrawIndexed.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbDraw/src/DrawNonIndexed.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbDraw/src/IndexBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbDraw/src/Instancing.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbDraw/src/Stats.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbFlow/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbFlow/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbFlow/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbFlow/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/ContextState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Display.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Init.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Marker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Predication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Registers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/DcbState/src/Workload.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Misc/src/PacketInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Misc/src/RegisterDefaults.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Misc/src/ShaderFusion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Misc/src/Suspend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/Branch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/CondExec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/Dma.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/IndirectRegisters.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/Jump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/Nop.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/QueueEndOfPipeAction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/Rewind.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Patch/src/WaitRegMem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Predication/src/Predication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Shader/src/CreateShader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Shader/src/InterpolantMapping.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Shader/src/LinkShaders.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Shader/src/PrimState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceAgc/Shader/src/ShaderUtils.cpp
)

target_include_directories(libSceAgcVsh PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAgcVsh PRIVATE libSceAgcDriver libc)

option(APS5_AGC_CREATE_LOG "Logging sceAgcCreateShader Ok status" ON)

target_compile_definitions(libSceAgcVsh PRIVATE
        APS5_AGC_CREATE_LOG=$<BOOL:${APS5_AGC_CREATE_LOG}>
)
