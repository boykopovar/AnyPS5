#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <cstddef>
#include "SceTypes.hpp"
#include "SceShaders.hpp"

namespace AgcDriver {

void Submit(const Packet* packet, std::uint32_t queue);

struct AwaitedWait {
    std::uint64_t address = 0;
    std::uint64_t reference = 0;
    std::uint64_t mask = 0;
    std::uint32_t function = 0;
    std::uint32_t bytes = 0;
};

}

extern "C" void AgcDriverPrepareCommandWrite_nid_no_patch(std::uint64_t address, std::size_t bytes);
extern "C" void AgcDriverWaitIdle_nid_postfix();
extern "C" void AgcDriverRaiseWorkerThreadPriority_nid_postfix(const char* role);
extern "C" void AgcDriverLockVulkanLoader_nid_postfix();
extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix();
extern "C" void AgcDriverShutdown_nid_postfix();
extern "C" void AgcDriverRequestScreenshot_nid_postfix();
extern "C" void AgcDriverSuspendPoint_nid_postfix();
extern "C" void AgcDriverSerialProgress_nid_postfix(std::uint64_t* accepted, std::uint64_t* completed);
extern "C" std::size_t AgcDriverAwaitedWaits_nid_postfix(AgcDriver::AwaitedWait* waits, std::size_t capacity);
extern "C" std::uint64_t AgcDriverDispatchesAdoptedAhead_nid_postfix();
extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader);
extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);
extern "C" void* AgcDriverStaticTable_nid_postfix();

#endif
