add_library(libSceAgc SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Control.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Branch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Registers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/src/Sync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Command/src/Memory.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Predication/src/Predication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/CreateShader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/InterpolantMapping.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/PrimState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/src/ShaderUtils.cpp
)

target_include_directories(libSceAgc PRIVATE ${LIBS_INCLUDE_DIR})
add_shader_recompiler(shader_recompiler_agc libc)
target_link_libraries(libSceAgc PRIVATE shader_recompiler_agc libc)
